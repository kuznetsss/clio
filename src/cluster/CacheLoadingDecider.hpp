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

#pragma once

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "etl/CacheLoadingState.hpp"

#include <boost/asio/thread_pool.hpp>

#include <memory>

namespace cluster {

/**
 * @brief Decides which node in the cluster should load the cache based on cluster state.
 *
 * This class monitors cluster state changes and determines whether the current node
 * should be allowed to load the cache. The decision is made by:
 * 1. Skipping if the current node has already loaded the cache
 * 2. Considering only nodes that have not yet loaded the cache
 * 3. Skipping if any of those nodes is currently loading the cache
 * 4. Sorting the remaining nodes by UUID and allowing cache loading if this node is first
 *
 * This ensures only one node in the cluster loads the cache at a time.
 */
class CacheLoadingDecider {
    /** @brief Thread pool for spawning asynchronous tasks */
    boost::asio::thread_pool& ctx_;

    /** @brief Interface for controlling the cache loading state of this node */
    std::unique_ptr<etl::CacheLoadingStateInterface> cacheLoadingState_;

public:
    /**
     * @brief Constructs a CacheLoadingDecider.
     *
     * @param ctx Thread pool for executing asynchronous operations
     * @param cacheLoadingState Cache loading state interface for controlling cache loading
     */
    CacheLoadingDecider(
        boost::asio::thread_pool& ctx,
        std::unique_ptr<etl::CacheLoadingStateInterface> cacheLoadingState
    );

    /**
     * @brief Handles cluster state changes and decides whether this node should load the cache.
     *
     * This method is called when cluster state changes. It asynchronously:
     * - Returns immediately if the current node has already loaded the cache
     * - Considers only nodes that have not yet loaded the cache
     * - Returns if any of those nodes is currently loading the cache
     * - Sorts the remaining nodes by UUID and allows cache loading if this node is first
     *
     * @param selfId The UUID of the current node
     * @param clusterData Shared pointer to current cluster data; may be empty if communication failed
     */
    void
    onNewState(ClioNode::CUuid selfId, std::shared_ptr<Backend::ClusterData const> clusterData);
};

}  // namespace cluster
