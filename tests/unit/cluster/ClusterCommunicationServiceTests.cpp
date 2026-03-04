//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "cluster/ClioNode.hpp"
#include "cluster/ClusterCommunicationService.hpp"
#include "data/BackendInterface.hpp"
#include "etl/SystemState.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockCacheLoadingState.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWriterState.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace cluster;

struct ClusterCommunicationServiceTest : util::prometheus::WithPrometheus, MockBackendTest {
    std::unique_ptr<NiceMockWriterState> writerState = std::make_unique<NiceMockWriterState>();
    NiceMockWriterState& writerStateRef = *writerState;

    std::unique_ptr<NiceMockCacheLoadingState> cacheLoadingState = std::make_unique<NiceMockCacheLoadingState>();
    NiceMockCacheLoadingState& cacheLoadingStateRef = *cacheLoadingState;

    static constexpr std::chrono::milliseconds kSHORT_INTERVAL{1};

    static boost::uuids::uuid
    makeUuid(uint8_t value)
    {
        boost::uuids::uuid uuid{};
        std::ranges::fill(uuid, value);
        return uuid;
    }

    static ClioNode
    makeNode(boost::uuids::uuid const& uuid, ClioNode::DbRole role)
    {
        return ClioNode{
            .uuid = std::make_shared<boost::uuids::uuid>(uuid),
            .updateTime = std::chrono::system_clock::now(),
            .dbRole = role,
            .isLoadingCache = false,
            .hasLoadedCache = true
        };
    }

    static ClioNode
    makeNode(boost::uuids::uuid const& uuid, ClioNode::DbRole role, bool isLoadingCache, bool hasLoadedCache)
    {
        return ClioNode{
            .uuid = std::make_shared<boost::uuids::uuid>(uuid),
            .updateTime = std::chrono::system_clock::now(),
            .dbRole = role,
            .isLoadingCache = isLoadingCache,
            .hasLoadedCache = hasLoadedCache
        };
    }

    static std::string
    nodeToJson(ClioNode const& node)
    {
        boost::json::value const v = boost::json::value_from(node);
        return boost::json::serialize(v);
    }

    ClusterCommunicationServiceTest()
    {
        ON_CALL(writerStateRef, clone()).WillByDefault(testing::Invoke([]() {
            auto state = std::make_unique<NiceMockWriterState>();
            ON_CALL(*state, isReadOnly()).WillByDefault(testing::Return(false));
            ON_CALL(*state, isWriting()).WillByDefault(testing::Return(true));
            return state;
        }));
        ON_CALL(writerStateRef, isReadOnly()).WillByDefault(testing::Return(false));
        ON_CALL(writerStateRef, isWriting()).WillByDefault(testing::Return(true));

        ON_CALL(cacheLoadingStateRef, clone()).WillByDefault(testing::Invoke([]() {
            auto state = std::make_unique<NiceMockCacheLoadingState>();
            ON_CALL(*state, isLoadingCache()).WillByDefault(testing::Return(false));
            ON_CALL(*state, hasLoadedCache()).WillByDefault(testing::Return(true));
            return state;
        }));
        ON_CALL(cacheLoadingStateRef, isLoadingCache()).WillByDefault(testing::Return(false));
        ON_CALL(cacheLoadingStateRef, hasLoadedCache()).WillByDefault(testing::Return(true));
    }

    static bool
    waitForSignal(std::binary_semaphore& sem, std::chrono::milliseconds timeout = std::chrono::milliseconds{1000})
    {
        return sem.try_acquire_for(timeout);
    }
};

TEST_F(ClusterCommunicationServiceTest, BackendReadsAndWritesData)
{
    auto const otherUuid = makeUuid(0x02);
    std::binary_semaphore fetchSemaphore{0};
    std::binary_semaphore writeSemaphore{0};

    BackendInterface::ClioNodesDataFetchResult fetchResult{std::vector<std::pair<boost::uuids::uuid, std::string>>{
        {otherUuid, nodeToJson(makeNode(otherUuid, ClioNode::DbRole::Writer))}
    }};

    ON_CALL(*backend_, fetchClioNodesData).WillByDefault(testing::Invoke([&](auto) {
        fetchSemaphore.release();
        return fetchResult;
    }));

    ON_CALL(*backend_, writeNodeMessage).WillByDefault(testing::Invoke([&](auto, auto) { writeSemaphore.release(); }));

    ClusterCommunicationService service{
        backend_, std::move(writerState), std::move(cacheLoadingState), kSHORT_INTERVAL, kSHORT_INTERVAL
    };

    service.run();

    EXPECT_TRUE(waitForSignal(fetchSemaphore));
    EXPECT_TRUE(waitForSignal(writeSemaphore));

    service.stop();
}

TEST_F(ClusterCommunicationServiceTest, MetricsGetsNewStateFromBackend)
{
    auto const otherUuid = makeUuid(0x02);
    std::binary_semaphore writerActionSemaphore{0};

    BackendInterface::ClioNodesDataFetchResult fetchResult{std::vector<std::pair<boost::uuids::uuid, std::string>>{
        {otherUuid, nodeToJson(makeNode(otherUuid, ClioNode::DbRole::Writer))}
    }};

    ON_CALL(*backend_, fetchClioNodesData).WillByDefault(testing::Invoke([&](auto) { return fetchResult; }));

    ON_CALL(writerStateRef, clone()).WillByDefault(testing::Invoke([&]() mutable {
        auto state = std::make_unique<NiceMockWriterState>();
        ON_CALL(*state, startWriting()).WillByDefault(testing::Invoke([&]() { writerActionSemaphore.release(); }));
        ON_CALL(*state, giveUpWriting()).WillByDefault(testing::Invoke([&]() { writerActionSemaphore.release(); }));
        return state;
    }));

    auto& nodesInClusterMetric = PrometheusService::gaugeInt("cluster_nodes_total_number", {});
    auto isHealthyMetric = PrometheusService::boolMetric("cluster_communication_is_healthy", {});

    ClusterCommunicationService service{
        backend_, std::move(writerState), std::move(cacheLoadingState), kSHORT_INTERVAL, kSHORT_INTERVAL
    };

    service.run();

    // WriterDecider is called after metrics are updated so we could use it as a signal to stop
    EXPECT_TRUE(waitForSignal(writerActionSemaphore));

    service.stop();

    EXPECT_EQ(nodesInClusterMetric.value(), 2);
    EXPECT_TRUE(static_cast<bool>(isHealthyMetric));
}

TEST_F(ClusterCommunicationServiceTest, WriterDeciderCallsWriterStateMethodsAccordingly)
{
    auto const smallerUuid = makeUuid(0x00);
    std::binary_semaphore fetchSemaphore{0};
    std::binary_semaphore writerActionSemaphore{0};

    BackendInterface::ClioNodesDataFetchResult fetchResult{std::vector<std::pair<boost::uuids::uuid, std::string>>{
        {smallerUuid, nodeToJson(makeNode(smallerUuid, ClioNode::DbRole::Writer))}
    }};

    ON_CALL(*backend_, fetchClioNodesData).WillByDefault(testing::Invoke([&](auto) {
        fetchSemaphore.release();
        return fetchResult;
    }));

    ON_CALL(*backend_, writeNodeMessage).WillByDefault(testing::Return());

    ON_CALL(writerStateRef, clone()).WillByDefault(testing::Invoke([&]() mutable {
        auto state = std::make_unique<NiceMockWriterState>();
        ON_CALL(*state, startWriting()).WillByDefault(testing::Invoke([&]() { writerActionSemaphore.release(); }));
        ON_CALL(*state, giveUpWriting()).WillByDefault(testing::Invoke([&]() { writerActionSemaphore.release(); }));
        return state;
    }));

    ClusterCommunicationService service{
        backend_, std::move(writerState), std::move(cacheLoadingState), kSHORT_INTERVAL, kSHORT_INTERVAL
    };

    service.run();

    EXPECT_TRUE(waitForSignal(fetchSemaphore));
    EXPECT_TRUE(waitForSignal(writerActionSemaphore));

    service.stop();
}

TEST_F(ClusterCommunicationServiceTest, StopHaltsBackendOperations)
{
    std::atomic<int> backendOperationsCount{0};
    std::binary_semaphore fetchSemaphore{0};

    BackendInterface::ClioNodesDataFetchResult fetchResult{std::vector<std::pair<boost::uuids::uuid, std::string>>{}};

    ON_CALL(*backend_, fetchClioNodesData).WillByDefault(testing::Invoke([&](auto) {
        backendOperationsCount++;
        fetchSemaphore.release();
        return fetchResult;
    }));
    ON_CALL(*backend_, writeNodeMessage).WillByDefault(testing::Invoke([&](auto&&, auto&&) {
        backendOperationsCount++;
    }));

    ClusterCommunicationService service{
        backend_, std::move(writerState), std::move(cacheLoadingState), kSHORT_INTERVAL, kSHORT_INTERVAL
    };

    service.run();
    EXPECT_TRUE(waitForSignal(fetchSemaphore));
    service.stop();

    auto const countAfterStop = backendOperationsCount.load();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    EXPECT_EQ(backendOperationsCount.load(), countAfterStop);
}

TEST_F(ClusterCommunicationServiceTest, CacheLoadingDeciderCallsCacheLoadingStateMethodsAccordingly)
{
    // Use the largest possible UUID for the other node so self's random UUID is reliably smaller,
    // making self first in the sorted not-loaded list and triggering allowCacheLoading().
    auto const largerUuid = makeUuid(0xFF);
    std::binary_semaphore allowLoadingSemaphore{0};

    BackendInterface::ClioNodesDataFetchResult fetchResult{std::vector<std::pair<boost::uuids::uuid, std::string>>{
        {largerUuid, nodeToJson(makeNode(largerUuid, ClioNode::DbRole::NotWriter, false, false))}
    }};

    ON_CALL(*backend_, fetchClioNodesData).WillByDefault(testing::Invoke([&](auto) { return fetchResult; }));
    ON_CALL(*backend_, writeNodeMessage).WillByDefault(testing::Return());

    // Self must appear as not-yet-loaded in the cluster data, so the Backend's clone must report
    // hasLoadedCache() = false when building self's ClioNode entry.
    ON_CALL(cacheLoadingStateRef, clone()).WillByDefault(testing::Invoke([&]() {
        auto state = std::make_unique<NiceMockCacheLoadingState>();
        ON_CALL(*state, hasLoadedCache()).WillByDefault(testing::Return(false));
        ON_CALL(*state, isLoadingCache()).WillByDefault(testing::Return(false));
        ON_CALL(*state, allowCacheLoading()).WillByDefault(testing::Invoke([&]() { allowLoadingSemaphore.release(); }));
        return state;
    }));

    ClusterCommunicationService service{
        backend_, std::move(writerState), std::move(cacheLoadingState), kSHORT_INTERVAL, kSHORT_INTERVAL
    };

    service.run();
    EXPECT_TRUE(waitForSignal(allowLoadingSemaphore));
    service.stop();
}

// Tests for ClusterCommunicationService::make() factory using real WriterState and CacheLoadingState.

struct ClusterCommunicationServiceMakeTestParams {
    std::string testName;
    bool limitLoadInCluster;
    bool expectedIsLoadingAllowed;
};

struct ClusterCommunicationServiceMakeTest
    : util::prometheus::WithPrometheus
    , MockBackendTest
    , testing::WithParamInterface<ClusterCommunicationServiceMakeTestParams> {
    std::shared_ptr<etl::SystemState> systemState = std::make_shared<etl::SystemState>();

    static util::config::ClioConfigDefinition
    makeConfig(bool limitLoadInCluster)
    {
        return util::config::ClioConfigDefinition{
            {"cache.limit_load_in_cluster",
             util::config::ConfigValue{util::config::ConfigType::Boolean}.defaultValue(limitLoadInCluster)}
        };
    }
};

TEST_P(ClusterCommunicationServiceMakeTest, IsLoadingAllowedMatchesConfig)
{
    auto const& params = GetParam();
    auto result = ClusterCommunicationService::make(makeConfig(params.limitLoadInCluster), backend_, systemState);
    EXPECT_EQ(result.cacheLoadingState->isLoadingAllowed(), params.expectedIsLoadingAllowed);
}

INSTANTIATE_TEST_SUITE_P(
    ClusterCommunicationServiceMakeTests,
    ClusterCommunicationServiceMakeTest,
    testing::Values(
        ClusterCommunicationServiceMakeTestParams{
            .testName = "LimitLoadFalseAllowsImmediately",
            .limitLoadInCluster = false,
            .expectedIsLoadingAllowed = true
        },
        ClusterCommunicationServiceMakeTestParams{
            .testName = "LimitLoadTrueDoesNotAllowImmediately",
            .limitLoadInCluster = true,
            .expectedIsLoadingAllowed = false
        }
    ),
    [](testing::TestParamInfo<ClusterCommunicationServiceMakeTestParams> const& info) { return info.param.testName; }
);
