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

#include "etl/SystemState.hpp"

#include <atomic>
#include <memory>

namespace etl {

/**
 * @brief Interface for managing cache loading state in the ETL subsystem.
 *
 * This interface provides methods to query and control whether the ETL process
 * has loaded its cache. Implementations coordinate with the ETL system state
 * to manage cache loading synchronization.
 */
class CacheLoadingStateInterface {
public:
    virtual ~CacheLoadingStateInterface() = default;

    /**
     * @brief Check if the cache has been fully loaded after startup.
     * @return true if the cache has finished loading, false otherwise
     */
    [[nodiscard]] virtual bool
    hasLoadedCache() const = 0;

    /**
     * @brief Check if the cache is currently in the process of loading.
     * @return true if the cache is currently loading, false otherwise
     */
    [[nodiscard]] virtual bool
    isLoadingCache() const = 0;

    /**
     * @brief Check if cache loading is currently permitted.
     * @return true if allowCacheLoading() has been called, false otherwise
     */
    [[nodiscard]] virtual bool
    isLoadingAllowed() const = 0;

    /**
     * @brief Block until cache loading is permitted.
     *
     * Waits until allowCacheLoading() has been called, then returns.
     * This is used to synchronize cache loading with cluster coordination.
     */
    virtual void
    waitForAllowedCacheLoading() const = 0;

    /**
     * @brief Allow cache loading to proceed.
     *
     * Signals any threads blocked in waitForAllowedCacheLoading() that they
     * may proceed with loading the cache.
     */
    virtual void
    allowCacheLoading() = 0;

    /**
     * @brief Create a clone of this cache loading state.
     *
     * Creates a new instance that shares the same underlying system state.
     * This is used when spawning operations that need their own state instance
     * while sharing the same system state.
     *
     * @note The clone has its own loadingAllowed flag, independent of the original.
     *
     * @return A unique pointer to the cloned state.
     */
    [[nodiscard]] virtual std::unique_ptr<CacheLoadingStateInterface>
    clone() const = 0;
};

/**
 * @brief Implementation of CacheLoadingStateInterface that manages cache loading state.
 *
 * This class coordinates with SystemState to track whether the ETL cache has been
 * loaded after startup. It also provides a mechanism to gate cache loading via
 * allowCacheLoading() and waitForAllowedCacheLoading(), which is used in cluster
 * deployments to prevent all nodes from loading cache simultaneously.
 */
class CacheLoadingState : public CacheLoadingStateInterface {
    std::shared_ptr<std::atomic_bool> loadingAllowed_ = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<SystemState const> state_;

    CacheLoadingState(std::shared_ptr<SystemState const> state, std::shared_ptr<std::atomic_bool> loadingAllowed);

public:
    /**
     * @brief Construct a CacheLoadingState with the given system state.
     * @param state Shared pointer to the (const) system state for coordination
     */
    explicit CacheLoadingState(std::shared_ptr<SystemState const> state);

    /**
     * @brief Check if the cache has been fully loaded after startup.
     * @return true if the cache has finished loading, false otherwise
     */
    [[nodiscard]] bool
    hasLoadedCache() const override;

    /**
     * @brief Check if the cache is currently in the process of loading.
     * @return true if the cache is currently loading, false otherwise
     */
    [[nodiscard]] bool
    isLoadingCache() const override;

    /**
     * @brief Check if cache loading is currently permitted.
     * @return true if allowCacheLoading() has been called, false otherwise
     */
    [[nodiscard]] bool
    isLoadingAllowed() const override;

    /**
     * @brief Block until cache loading is permitted.
     *
     * Uses atomic wait to avoid busy-waiting. Blocks until allowCacheLoading() is called.
     */
    void
    waitForAllowedCacheLoading() const override;

    /**
     * @brief Allow cache loading to proceed.
     *
     * Atomically sets the allowed flag and notifies all threads waiting in
     * waitForAllowedCacheLoading().
     */
    void
    allowCacheLoading() override;

    /**
     * @brief Create a clone sharing the same system state and loadingAllowed flag.
     *
     * The clone shares both the same SystemState and the same loadingAllowed flag,
     * so calling allowCacheLoading() on any instance (original or clone) unblocks
     * all threads waiting on any instance that shares the flag.
     *
     * @return A unique pointer to the cloned state.
     */
    [[nodiscard]] std::unique_ptr<CacheLoadingStateInterface>
    clone() const override;
};

}  // namespace etl
