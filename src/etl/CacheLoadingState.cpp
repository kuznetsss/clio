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

#include <memory>
#include <utility>

namespace etl {

CacheLoadingState::CacheLoadingState(std::shared_ptr<SystemState const> state) : state_(std::move(state))
{
}

bool
CacheLoadingState::hasLoadedCache() const
{
    return state_->hasLoadedCache;
}

bool
CacheLoadingState::isLoadingCache() const
{
    return state_->isLoadingCache;
}

void
CacheLoadingState::waitForAllowedCacheLoading() const
{
    loadingAllowed_->wait(false);
}

void
CacheLoadingState::allowCacheLoading()
{
    *loadingAllowed_ = true;
    loadingAllowed_->notify_all();
}

std::unique_ptr<CacheLoadingStateInterface>
CacheLoadingState::clone() const
{
    return std::make_unique<CacheLoadingState>(state_);
}

}  // namespace etl
