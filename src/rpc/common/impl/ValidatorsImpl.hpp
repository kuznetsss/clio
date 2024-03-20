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

#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/value.hpp>
#include <ripple/protocol/ErrorCodes.h>

#include <string>
#include <string_view>

namespace rpc::validation::impl {

/**
 * @brief A validator that forbids a field to be present.
 *
 * If there is a value provided, it will forbid the field only when the value equals.
 * If there is no value provided, it will forbid the field when the field shows up.
 */
template <typename ErrorStrategy, typename... T>
class BadField;

/**
 * @brief A specialized BadField validator that forbids a field to be present when the value equals the given value.
 */
template <typename ErrorStrategy, typename T>
class BadField<ErrorStrategy, T> final {
    T value_;

public:
    /**
     * @brief Constructs a new BadField validator.
     *
     * @param val The value to store and verify against
     */
    BadField(T val) : value_(val)
    {
    }

    /**
     * @brief Verify whether the field is supported or not.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcNOT_SUPPORTED` if the value matched; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (value.is_object() and value.as_object().contains(key.data())) {
            using boost::json::value_to;
            auto const res = value_to<T>(value.as_object().at(key.data()));
            if (value_ == res) {
                return ErrorStrategy::makeError(key, res);
                // return Error{Status{
                //     RippledError::rpcNOT_SUPPORTED,
                //     fmt::format("Not supported field '{}'s value '{}'", std::string{key}, res)
                // }};
            }
        }
        return {};
    }
};

/**
 * @brief A specialized BadField validator that forbids a field to be present.
 */
template <typename ErrorStrategy>
class BadField<ErrorStrategy> final {
public:
    /**
     * @brief Verify whether the field is supported or not.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcNOT_SUPPORTED` if the field is found; otherwise no error is returned
     */
    [[nodiscard]] static MaybeError
    verify(boost::json::value const& value, std::string_view key)
    {
        if (value.is_object() and value.as_object().contains(key))
            return ErrorStrategy::makeError(key);
        // return Error{Status{RippledError::rpcNOT_SUPPORTED, "Not supported field '" + std::string{key}}};

        return {};
    }
};

/**
 * @brief Deduction guide to avoid having to specify the template arguments.
 */
template <typename ErrorStrategy, typename... T>
BadField<ErrorStrategy>(T&&... t)->BadField<ErrorStrategy, T...>;

struct NotSupportedErrorStrategy {
    static MaybeError
    makeError(std::string_view key)
    {
        return Error{Status{RippledError::rpcNOT_SUPPORTED, "Not supported field '" + std::string{key}}};
    }

    template <typename T>
    static MaybeError
    makeError(std::string_view key, T const& value)
    {
        return Error{Status{
            RippledError::rpcNOT_SUPPORTED, fmt::format("Not supported field '{}'s value '{}'", std::string{key}, value)
        }};
    }
};

}  // namespace rpc::validation::impl
