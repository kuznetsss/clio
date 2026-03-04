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

#include "cluster/CacheLoadingDecider.hpp"

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "etl/CacheLoadingState.hpp"
#include "util/Assert.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/thread_pool.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace cluster {

CacheLoadingDecider::CacheLoadingDecider(
    boost::asio::thread_pool& ctx,
    std::unique_ptr<etl::CacheLoadingStateInterface> cacheLoadingState
)
    : ctx_(ctx), cacheLoadingState_(std::move(cacheLoadingState))
{
}

void
CacheLoadingDecider::onNewState(ClioNode::CUuid selfId, std::shared_ptr<Backend::ClusterData const> clusterData)
{
    if (not clusterData->has_value())
        return;

    util::spawn(
        ctx_,
        [cacheLoadingState = cacheLoadingState_->clone(),
         selfId = std::move(selfId),
         clusterData = clusterData->value()](auto&&) mutable {
            auto const selfData =
                std::ranges::find_if(clusterData, [&selfId](ClioNode const& node) { return node.uuid == selfId; });
            ASSERT(selfData != clusterData.end(), "Self data should always be in the cluster data");

            if (selfData->hasLoadedCache or selfData->isLoadingCache)
                return;

            std::vector<ClioNode> notLoaded;
            std::ranges::copy_if(clusterData, std::back_inserter(notLoaded), [](ClioNode const& node) {
                return not node.hasLoadedCache;
            });

            if (std::ranges::any_of(notLoaded, [](ClioNode const& node) { return node.isLoadingCache; }))
                return;

            std::ranges::sort(notLoaded, [](ClioNode const& lhs, ClioNode const& rhs) {
                return *lhs.uuid < *rhs.uuid;
            });

            if (not notLoaded.empty() and *notLoaded.front().uuid == *selfId)
                cacheLoadingState->allowCacheLoading();
        }
    );
}

}  // namespace cluster
