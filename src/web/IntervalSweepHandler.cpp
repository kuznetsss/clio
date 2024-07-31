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

#include "web/IntervalSweepHandler.hpp"

#include "util/SignalsHandlerInterface.hpp"
#include "util/config/Config.hpp"
#include "web/DOSGuard.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/system/detail/error_code.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>

namespace web {

IntervalSweepHandler::IntervalSweepHandler(
    util::Config const& config,
    boost::asio::io_context& ctx,
    web::BaseDOSGuard& dosGuard,
    util::SignalsHandlerInterface& signalsHandler
)
    : sweepInterval_{std::max(
          std::chrono::milliseconds{1u},
          util::Config::toMilliseconds(config.valueOr("dos_guard.sweep_interval", 1.0))
      )}
    , ctx_{std::ref(ctx)}
    , timer_{ctx.get_executor()}
    , dosGuard_{dosGuard}
{
    createTimer();
    signalsHandler.subscribeToStop([this]() { this->stop(); });
}

IntervalSweepHandler::~IntervalSweepHandler()
{
    if (cancelOperation_.valid())
        cancelOperation_.wait();
}

void
IntervalSweepHandler::stop()
{
    cancelOperation_ = boost::asio::spawn(
        ctx_.get(),
        [this](auto&&) {
            timer_.cancel();
            stopped_ = true;
        },
        boost::asio::use_future
    );
}

void
IntervalSweepHandler::createTimer()
{
    timer_.expires_after(sweepInterval_);
    timer_.async_wait([this](boost::system::error_code const& error) {
        if (error == boost::asio::error::operation_aborted || stopped_) {
            return;
        }

        dosGuard_.get().clear();
        boost::asio::post(ctx_.get(), [this] { createTimer(); });
    });
}

}  // namespace web
