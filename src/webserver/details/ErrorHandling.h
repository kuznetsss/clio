//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <rpc/Errors.h>
#include <webserver/interface/ConnectionBase.h>

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <memory>
#include <optional>
#include <string>

namespace Server::detail {

/**
 * @brief A helper that attempts to match rippled reporting mode HTTP errors as close as possible.
 */
class ErrorHelper
{
    std::shared_ptr<Server::ConnectionBase> connection_;
    std::optional<boost::json::object> request_;

public:
    ErrorHelper(
        std::shared_ptr<Server::ConnectionBase> const& connection,
        std::optional<boost::json::object> request = std::nullopt)
        : connection_{connection}, request_{std::move(request)}
    {
    }

    void
    sendError(RPC::Status const& err) const
    {
        if (connection_->upgraded)
        {
            connection_->send(boost::json::serialize(composeError(err)));
        }
        else
        {
            // Note: a collection of crutches to match rippled output follows
            if (auto const clioCode = std::get_if<RPC::ClioError>(&err.code))
            {
                switch (*clioCode)
                {
                    case RPC::ClioError::rpcINVALID_API_VERSION:
                        connection_->send(
                            std::string{RPC::getErrorInfo(*clioCode).error}, boost::beast::http::status::bad_request);
                        break;
                    case RPC::ClioError::rpcCOMMAND_IS_MISSING:
                        connection_->send("Null method", boost::beast::http::status::bad_request);
                        break;
                    case RPC::ClioError::rpcCOMMAND_IS_EMPTY:
                        connection_->send("method is empty", boost::beast::http::status::bad_request);
                        break;
                    case RPC::ClioError::rpcCOMMAND_NOT_STRING:
                        connection_->send("method is not string", boost::beast::http::status::bad_request);
                        break;
                    case RPC::ClioError::rpcPARAMS_UNPARSEABLE:
                        connection_->send("params unparseable", boost::beast::http::status::bad_request);
                        break;

                    // others are not applicable but we want a compilation error next time we add one
                    case RPC::ClioError::rpcUNKNOWN_OPTION:
                    case RPC::ClioError::rpcMALFORMED_CURRENCY:
                    case RPC::ClioError::rpcMALFORMED_REQUEST:
                    case RPC::ClioError::rpcMALFORMED_OWNER:
                    case RPC::ClioError::rpcMALFORMED_ADDRESS:
                    case RPC::ClioError::rpcINVALID_HOT_WALLET:
                        assert(false);  // this should never happen
                        break;
                }
            }
            else
            {
                connection_->send(boost::json::serialize(composeError(err)), boost::beast::http::status::bad_request);
            }
        }
    }

    void
    sendInternalError() const
    {
        connection_->send(
            boost::json::serialize(composeError(RPC::RippledError::rpcINTERNAL)),
            boost::beast::http::status::internal_server_error);
    }

    void
    sendNotReadyError() const
    {
        connection_->send(
            boost::json::serialize(composeError(RPC::RippledError::rpcNOT_READY)), boost::beast::http::status::ok);
    }

    void
    sendTooBusyError() const
    {
        if (connection_->upgraded)
            connection_->send(
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcTOO_BUSY)), boost::beast::http::status::ok);
        else
            connection_->send(
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcTOO_BUSY)),
                boost::beast::http::status::service_unavailable);
    }

    void
    sendJsonParsingError(std::string_view reason) const
    {
        if (connection_->upgraded)
            connection_->send(
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcBAD_SYNTAX)),
                boost::beast::http::status::ok);
        else
            connection_->send(
                fmt::format("Unable to parse request: {}", reason), boost::beast::http::status::bad_request);
    }

    boost::json::object
    composeError(auto const& error) const
    {
        auto e = RPC::makeError(error);

        if (request_)
        {
            auto const& req = request_.value();
            auto const id = req.contains("id") ? req.at("id") : nullptr;
            if (not id.is_null())
                e["id"] = id;

            e["request"] = req;
        }

        if (connection_->upgraded)
            return e;
        else
            return {{"result", e}};
    }
};

}  // namespace Server::detail
