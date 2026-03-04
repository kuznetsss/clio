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

#include "cluster/Backend.hpp"
#include "cluster/CacheLoadingDecider.hpp"
#include "cluster/ClioNode.hpp"
#include "util/MockCacheLoadingState.hpp"

#include <boost/asio/thread_pool.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace cluster;

struct CacheLoadingDeciderTestNodeParams {
    uint8_t uuidValue;
    bool isLoadingCache;
    bool hasLoadedCache;
};

struct CacheLoadingDeciderTestParams {
    std::string testName;
    uint8_t selfUuidValue;
    std::vector<CacheLoadingDeciderTestNodeParams> nodes;
    bool expectAllowCacheLoading;
    bool useEmptyClusterData = false;
};

struct CacheLoadingDeciderTest : testing::TestWithParam<CacheLoadingDeciderTestParams> {
    ~CacheLoadingDeciderTest() override
    {
        ctx.stop();
        ctx.join();
    }

    boost::asio::thread_pool ctx{1};
    std::unique_ptr<MockCacheLoadingState> cacheLoadingState = std::make_unique<MockCacheLoadingState>();
    MockCacheLoadingState& cacheLoadingStateRef = *cacheLoadingState;

    static ClioNode
    makeNode(boost::uuids::uuid const& uuid, bool isLoadingCache, bool hasLoadedCache)
    {
        return ClioNode{
            .uuid = std::make_shared<boost::uuids::uuid>(uuid),
            .updateTime = std::chrono::system_clock::now(),
            .dbRole = ClioNode::DbRole::NotWriter,
            .isLoadingCache = isLoadingCache,
            .hasLoadedCache = hasLoadedCache
        };
    }

    static boost::uuids::uuid
    makeUuid(uint8_t value)
    {
        boost::uuids::uuid uuid{};
        std::ranges::fill(uuid, value);
        return uuid;
    }
};

TEST_P(CacheLoadingDeciderTest, CacheLoadingDecision)
{
    auto const& params = GetParam();

    auto const selfUuid = makeUuid(params.selfUuidValue);

    CacheLoadingDecider decider{ctx, std::move(cacheLoadingState)};

    auto clonedState = std::make_unique<MockCacheLoadingState>();

    if (params.useEmptyClusterData) {
        // clone() is never called when cluster data is empty
    } else if (params.expectAllowCacheLoading) {
        EXPECT_CALL(*clonedState, allowCacheLoading());
        EXPECT_CALL(cacheLoadingStateRef, clone()).WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
    } else {
        EXPECT_CALL(cacheLoadingStateRef, clone()).WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
    }

    std::shared_ptr<Backend::ClusterData> clusterData;
    ClioNode::CUuid selfIdPtr;

    if (params.useEmptyClusterData) {
        clusterData = std::make_shared<Backend::ClusterData>(std::unexpected(std::string("Communication failed")));
        selfIdPtr = std::make_shared<boost::uuids::uuid>(selfUuid);
    } else {
        std::vector<ClioNode> nodes;
        nodes.reserve(params.nodes.size());
        for (auto const& nodeParams : params.nodes) {
            auto node = makeNode(makeUuid(nodeParams.uuidValue), nodeParams.isLoadingCache, nodeParams.hasLoadedCache);
            if (nodeParams.uuidValue == params.selfUuidValue) {
                selfIdPtr = node.uuid;
            }
            nodes.push_back(std::move(node));
        }
        clusterData = std::make_shared<Backend::ClusterData>(std::move(nodes));
    }

    decider.onNewState(selfIdPtr, clusterData);

    ctx.join();
}

INSTANTIATE_TEST_SUITE_P(
    CacheLoadingDeciderTests,
    CacheLoadingDeciderTest,
    testing::Values(
        CacheLoadingDeciderTestParams{
            .testName = "SelfHasLoadedCacheNoAction",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = true},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = false
        },
        CacheLoadingDeciderTestParams{
            .testName = "AllNodesLoadedCacheNoAction",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = true},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = true}},
            .expectAllowCacheLoading = false
        },
        CacheLoadingDeciderTestParams{
            .testName = "SelfIsFirstByUuidAllowCacheLoading",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = true
        },
        CacheLoadingDeciderTestParams{
            .testName = "OtherNodeIsFirstByUuidNoAction",
            .selfUuidValue = 0x02,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = false
        },
        CacheLoadingDeciderTestParams{
            .testName = "SomeoneInNotLoadedGroupIsLoadingNoAction",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x02, .isLoadingCache = true, .hasLoadedCache = false}},
            .expectAllowCacheLoading = false
        },
        CacheLoadingDeciderTestParams{
            .testName = "SelfIsLoadingNoAction",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = true, .hasLoadedCache = false},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = false
        },
        CacheLoadingDeciderTestParams{
            .testName = "EmptyClusterDataNoAction",
            .selfUuidValue = 0x01,
            .nodes = {},
            .expectAllowCacheLoading = false,
            .useEmptyClusterData = true
        },
        CacheLoadingDeciderTestParams{
            .testName = "SingleNodeAllowCacheLoading",
            .selfUuidValue = 0x01,
            .nodes = {{.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = true
        },
        CacheLoadingDeciderTestParams{
            .testName = "NodesAreSortedByUuidSelfNotFirst",
            .selfUuidValue = 0x03,
            .nodes =
                {{.uuidValue = 0x03, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = false
        },
        CacheLoadingDeciderTestParams{
            .testName = "NodesAreSortedByUuidSelfIsFirst",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x03, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = true
        },
        CacheLoadingDeciderTestParams{
            .testName = "LoadedNodesExcludedSelfIsFirstAmongNotLoaded",
            // Node 0x01 has loaded cache so is excluded; self (0x02) is first among not-loaded
            .selfUuidValue = 0x02,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = false, .hasLoadedCache = true},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false},
                 {.uuidValue = 0x03, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = true
        },
        CacheLoadingDeciderTestParams{
            .testName = "LoadingAmongLoadedNodesIsIgnored",
            // Node 0x01 has loaded cache (even though isLoadingCache=true, it's excluded from the group)
            .selfUuidValue = 0x02,
            .nodes =
                {{.uuidValue = 0x01, .isLoadingCache = true, .hasLoadedCache = true},
                 {.uuidValue = 0x02, .isLoadingCache = false, .hasLoadedCache = false}},
            .expectAllowCacheLoading = true
        }
    ),
    [](testing::TestParamInfo<CacheLoadingDeciderTestParams> const& info) { return info.param.testName; }
);
