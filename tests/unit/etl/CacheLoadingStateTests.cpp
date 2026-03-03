//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2026, the clio developers.

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

#include "etl/CacheLoadingState.hpp"
#include "etl/SystemState.hpp"
#include "util/MockPrometheus.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <semaphore>
#include <thread>

using namespace etl;

struct CacheLoadingStateTest : util::prometheus::WithPrometheus {
    std::shared_ptr<SystemState> systemState = std::make_shared<SystemState>();
    CacheLoadingState cacheLoadingState{systemState};
};

TEST_F(CacheLoadingStateTest, HasLoadedCacheReturnsSystemStateValue)
{
    systemState->hasLoadedCache = false;
    EXPECT_FALSE(cacheLoadingState.hasLoadedCache());

    systemState->hasLoadedCache = true;
    EXPECT_TRUE(cacheLoadingState.hasLoadedCache());
}

TEST_F(CacheLoadingStateTest, IsLoadingCacheReturnsSystemStateValue)
{
    systemState->isLoadingCache = false;
    EXPECT_FALSE(cacheLoadingState.isLoadingCache());

    systemState->isLoadingCache = true;
    EXPECT_TRUE(cacheLoadingState.isLoadingCache());
}

TEST_F(CacheLoadingStateTest, WaitReturnsImmediatelyAfterAllowCacheLoading)
{
    cacheLoadingState.allowCacheLoading();

    std::binary_semaphore finished{0};
    std::thread runner([&]() {
        cacheLoadingState.waitForAllowedCacheLoading();
        finished.release();
    });

    EXPECT_TRUE(finished.try_acquire_for(std::chrono::milliseconds{1000}));
    runner.join();
}

TEST_F(CacheLoadingStateTest, AllowCacheLoadingUnblocksWaitingThread)
{
    std::binary_semaphore aboutToWait{0};
    std::binary_semaphore finished{0};

    std::thread waiter([&]() {
        aboutToWait.release();
        cacheLoadingState.waitForAllowedCacheLoading();
        finished.release();
    });

    // Wait until the thread is about to enter waitForAllowedCacheLoading,
    // then verify it is blocking before we allow it.
    aboutToWait.acquire();
    EXPECT_FALSE(finished.try_acquire_for(std::chrono::milliseconds{5}));

    cacheLoadingState.allowCacheLoading();

    EXPECT_TRUE(finished.try_acquire_for(std::chrono::milliseconds{1000}));
    waiter.join();
}

TEST_F(CacheLoadingStateTest, AllowCacheLoadingUnblocksMultipleWaitingThreads)
{
    std::binary_semaphore finished1{0};
    std::binary_semaphore finished2{0};

    std::thread waiter1([&]() {
        cacheLoadingState.waitForAllowedCacheLoading();
        finished1.release();
    });
    std::thread waiter2([&]() {
        cacheLoadingState.waitForAllowedCacheLoading();
        finished2.release();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{5});

    cacheLoadingState.allowCacheLoading();

    EXPECT_TRUE(finished1.try_acquire_for(std::chrono::milliseconds{1000}));
    EXPECT_TRUE(finished2.try_acquire_for(std::chrono::milliseconds{1000}));
    waiter1.join();
    waiter2.join();
}

TEST_F(CacheLoadingStateTest, CloneSharesSystemState)
{
    systemState->isLoadingCache = true;
    systemState->hasLoadedCache = false;

    auto cloned = cacheLoadingState.clone();

    ASSERT_NE(cloned.get(), &cacheLoadingState);
    EXPECT_TRUE(cloned->isLoadingCache());
    EXPECT_FALSE(cloned->hasLoadedCache());

    systemState->isLoadingCache = false;
    systemState->hasLoadedCache = true;

    EXPECT_FALSE(cloned->isLoadingCache());
    EXPECT_TRUE(cloned->hasLoadedCache());
}

TEST_F(CacheLoadingStateTest, CloneHasIndependentLoadingAllowedFlag)
{
    auto cloned = cacheLoadingState.clone();

    // Allow on the original should NOT unblock the clone's wait
    cacheLoadingState.allowCacheLoading();

    // Clone still has its own loadingAllowed_ set to false
    EXPECT_FALSE(cloned->clone()->isLoadingCache());  // Just verifies clone is functional

    // Allow on the clone and verify it unblocks the clone's wait
    std::binary_semaphore finished{0};
    std::thread waiter([&]() {
        cloned->waitForAllowedCacheLoading();
        finished.release();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{5});
    EXPECT_FALSE(finished.try_acquire_for(std::chrono::milliseconds{0}));

    cloned->allowCacheLoading();

    EXPECT_TRUE(finished.try_acquire_for(std::chrono::milliseconds{1000}));
    waiter.join();
}
