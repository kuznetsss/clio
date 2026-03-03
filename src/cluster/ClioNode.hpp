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

#pragma once

#include "etl/CacheLoadingState.hpp"
#include "etl/WriterState.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <memory>

namespace cluster {

/**
 * @brief Represents a node in the cluster.
 */
struct ClioNode {
    /**
     * @brief The format of the time to store in the database.
     */
    static constexpr char const* kTIME_FORMAT = "%Y-%m-%dT%H:%M:%SZ";

    /**
     * @brief Database role of a node in the cluster.
     *
     * Roles are used to coordinate which node writes to the database:
     * - ReadOnly: Node is configured to never write (strict read-only mode)
     * - NotLoadedCache: Node has not yet finished loading its cache and cannot write
     * - NotWriter: Node can write but is currently not the designated writer
     * - Writer: Node is actively writing to the database
     * - Fallback: Node is using the fallback writer decision mechanism
     *
     * When any node in the cluster is in Fallback mode, the entire cluster switches
     * from the cluster communication mechanism to the slower but more reliable
     * database-based conflict detection mechanism.
     */
    enum class DbRole { ReadOnly = 0, NotLoadedCache = 1, NotWriter = 2, Writer = 3, Fallback = 4, MAX = 4 };

    using Uuid = std::shared_ptr<boost::uuids::uuid>;
    using CUuid = std::shared_ptr<boost::uuids::uuid const>;

    Uuid uuid;                                         ///< The UUID of the node.
    std::chrono::system_clock::time_point updateTime;  ///< The time the data about the node was last updated.
    DbRole dbRole;       ///< The database role of the node.
    bool isLoadingCache; ///< Whether the node is currently loading its cache.
    bool hasLoadedCache; ///< Whether the node has finished loading its cache after startup.

    /**
     * @brief Create a ClioNode from writer and cache loading state.
     *
     * @param uuid The UUID of the node
     * @param writerState The writer state used to determine the node's database role
     * @param cacheLoadingState The cache loading state used to populate isLoadingCache and hasLoadedCache
     * @return A ClioNode with the current time, role derived from writerState, and cache fields from cacheLoadingState
     */
    static ClioNode
    from(Uuid uuid, etl::WriterStateInterface const& writerState, etl::CacheLoadingStateInterface const& cacheLoadingState);
};

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ClioNode const& node);

ClioNode
tag_invoke(boost::json::value_to_tag<ClioNode>, boost::json::value const& jv);

}  // namespace cluster
