//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include <csignal>
#include <functional>

namespace util {

/**
 * @brief Interface for handling signals.
 */
class SignalsHandlerInterface {
public:
    /**
     * @brief Enum for stop priority.
     */
    enum class Priority { StopFirst = 0, Normal = 1, StopLast = 2 };

    /**
     * @brief Callback type for signals.
     */
    using StopCallback = std::function<void()>;

    /**
     * @brief Destructor of SignalsHandler.
     */
    virtual ~SignalsHandlerInterface() = default;

    /**
     * @brief Subscribe to stop signal.
     *
     * @tparam SomeCallback The type of the callback.
     * @param callback The callback to call on stop signal.
     * @param priority The priority of the callback. Default is Normal.
     */
    virtual void
    subscribeToStop(StopCallback const& callback, Priority priority = Priority::Normal) = 0;

    static constexpr auto HANDLED_SIGNALS = {SIGINT, SIGTERM};
};

}  // namespace util
