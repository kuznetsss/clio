//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/requests/Types.h"

#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>

#include <string>
#include <utility>

namespace util::requests {

RequestError::RequestError(std::string message) : message(std::move(message))
{
}

RequestError::RequestError(std::string msg, boost::beast::error_code const& ec) : message(std::move(msg))
{
    message.append(": ");
    message.append(ec.message());
}

HttpHeader::HttpHeader(boost::beast::http::field name, std::string value) : name(name), value(std::move(value))
{
}

HttpHeader::HttpHeader(std::string name, std::string value) : name(std::move(name)), value(std::move(value))
{
}

}  // namespace util::requests
