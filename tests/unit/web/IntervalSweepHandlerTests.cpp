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

#include "util/AsioContextTestFixture.hpp"
#include "util/MockSignalsHandler.hpp"
#include "util/SignalsHandlerInterface.hpp"
#include "util/config/Config.hpp"
#include "web/DOSGuard.hpp"
#include "web/IntervalSweepHandler.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>

using namespace web;
using testing::AtLeast;
using testing::SaveArg;

struct IntervalSweepHandlerTest : SyncAsioContextTest {
    constexpr static auto JSONData = R"JSON(
    {
        "dos_guard": {
            "sweep_interval": 0.01
        }
    }
)JSON";

    struct BasicDOSGuardMock : web::BaseDOSGuard {
        MOCK_METHOD(void, clear, (), (noexcept, override));
    };

    util::Config cfg{boost::json::parse(JSONData)};
    testing::StrictMock<BasicDOSGuardMock> dosGuardMock;
    testing::StrictMock<SignalsHandlerMock> signalsHandlerMock;
};

TEST_F(IntervalSweepHandlerTest, SweepAfterInterval)
{
    EXPECT_CALL(signalsHandlerMock, subscribeToStop);
    IntervalSweepHandler sweepHandler{cfg, ctx, dosGuardMock, signalsHandlerMock};
    EXPECT_CALL(dosGuardMock, clear()).Times(AtLeast(2));  // should be 4 but in case test is slow
    ctx.run_for(std::chrono::milliseconds(40));
}

TEST_F(IntervalSweepHandlerTest, Stop)
{
    util::SignalsHandlerInterface::StopCallback stopCallback;
    EXPECT_CALL(signalsHandlerMock, subscribeToStop).WillOnce(SaveArg<0>(&stopCallback));
    IntervalSweepHandler sweepHandler{cfg, ctx, dosGuardMock, signalsHandlerMock};

    std::promise<void> completed;
    auto future = completed.get_future();
    EXPECT_CALL(dosGuardMock, clear()).Times(AtLeast(2));
    std::thread thread{[&]() {
        ctx.run();
        completed.set_value();
    }};
    std::this_thread::sleep_for(std::chrono::milliseconds{40});
    stopCallback();
    if (future.wait_for(std::chrono::seconds{1}) == std::future_status::timeout) {
        FAIL() << "Timeout: io_context didn't stop in 1 second";
    }
    thread.join();
}
