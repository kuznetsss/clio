//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include "rpc/common/impl/ValidatorsImpl.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <ripple/protocol/ErrorCodes.h>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rpc::validation {

/**
 * @brief Check that the type is the same as what was expected.
 *
 * @tparam Expected The expected type that value should be convertible to
 * @param value The json value to check the type of
 * @return true if convertible; false otherwise
 */
template <typename Expected>
[[nodiscard]] bool static checkType(boost::json::value const& value)
{
    auto hasError = false;
    if constexpr (std::is_same_v<Expected, bool>) {
        if (not value.is_bool())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, std::string>) {
        if (not value.is_string())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, double> or std::is_same_v<Expected, float>) {
        if (not value.is_double())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, boost::json::array>) {
        if (not value.is_array())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, boost::json::object>) {
        if (not value.is_object())
            hasError = true;
    } else if constexpr (std::is_convertible_v<Expected, uint64_t> or std::is_convertible_v<Expected, int64_t>) {
        if (not value.is_int64() && not value.is_uint64())
            hasError = true;
        // specify the type is unsigened, it can not be negative
        if constexpr (std::is_unsigned_v<Expected>) {
            if (value.is_int64() and value.as_int64() < 0)
                hasError = true;
        }
    }

    return not hasError;
}

/**
 * @brief A validator that simply requires a field to be present.
 */
struct Required final {
    /**
     * @brief Verify that the JSON value is present and not null.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return An error if validation failed; otherwise no error is returned
     */
    [[nodiscard]] static MaybeError
    verify(boost::json::value const& value, std::string_view key);
};

template <typename... T>
using NotSupported = impl::BadField<impl::NotSupportedErrorStrategy, T...>;

/**
 * @brief Validates that the type of the value is one of the given types.
 */
template <typename... Types>
struct Type final {
    /**
     * @brief Verify that the JSON value is (one) of specified type(s).
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const& res = value.as_object().at(key);
        auto const convertible = (checkType<Types>(res) || ...);

        if (not convertible)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value is between specified min and max.
 */
template <typename Type>
class Between final {
    Type min_;
    Type max_;

public:
    /**
     * @brief Construct the validator storing min and max values.
     *
     * @param min
     * @param max
     */
    explicit Between(Type min, Type max) : min_{min}, max_{max}
    {
    }

    /**
     * @brief Verify that the JSON value is within a certain range.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));

        // TODO: may want a way to make this code more generic (e.g. use a free
        // function that can be overridden for this comparison)
        if (res < min_ || res > max_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value is equal or greater than the specified min.
 */
template <typename Type>
class Min final {
    Type min_;

public:
    /**
     * @brief Construct the validator storing min value.
     *
     * @param min
     */
    explicit Min(Type min) : min_{min}
    {
    }

    /**
     * @brief Verify that the JSON value is not smaller than min
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));

        if (res < min_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value is not greater than max.
 */
template <typename Type>
class Max final {
    Type max_;

public:
    /**
     * @brief Construct the validator storing max value.
     *
     * @param max
     */
    explicit Max(Type max) : max_{max}
    {
    }

    /**
     * @brief Verify that the JSON value is not greater than max.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));

        if (res > max_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validates that the value is equal to the one passed in.
 */
template <typename Type>
class EqualTo final {
    Type original_;

public:
    /**
     * @brief Construct the validator with stored original value.
     *
     * @param original The original value to store
     */
    explicit EqualTo(Type original) : original_{std::move(original)}
    {
    }

    /**
     * @brief Verify that the JSON value is equal to the stored original.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));
        if (res != original_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Deduction guide to help disambiguate what it means to EqualTo a "string" without specifying the type.
 */
EqualTo(char const*) -> EqualTo<std::string>;

/**
 * @brief Validates that the value is one of the values passed in.
 */
template <typename Type>
class OneOf final {
    std::vector<Type> options_;

public:
    /**
     * @brief Construct the validator with stored options of initializer list.
     *
     * @param options The list of allowed options
     */
    explicit OneOf(std::initializer_list<Type> options) : options_{options}
    {
    }

    /**
     * @brief Construct the validator with stored options of other container.
     *
     * @param begin,end the range to copy the elements from
     */
    template <typename InputIt>
    explicit OneOf(InputIt begin, InputIt end) : options_{begin, end}
    {
    }

    /**
     * @brief Verify that the JSON value is one of the stored options.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));
        if (std::find(std::begin(options_), std::end(options_), res) == std::end(options_))
            return Error{Status{RippledError::rpcINVALID_PARAMS, fmt::format("Invalid field '{}'.", key)}};

        return {};
    }
};

/**
 * @brief Deduction guide to help disambiguate what it means to OneOf a few "strings" without specifying the type.
 */
OneOf(std::initializer_list<char const*>) -> OneOf<std::string>;

/**
 * @brief A meta-validator that allows to specify a custom validation function.
 */
class CustomValidator final {
    std::function<MaybeError(boost::json::value const&, std::string_view)> validator_;

public:
    /**
     * @brief Constructs a custom validator from any supported callable.
     *
     * @tparam Fn The type of callable
     * @param fn The callable/function object
     */
    template <typename Fn>
    explicit CustomValidator(Fn&& fn) : validator_{std::forward<Fn>(fn)}
    {
    }

    /**
     * @brief Verify that the JSON value is valid according to the custom validation function stored.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return Any compatible user-provided error if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief Helper function to check if input value is an uint32 number or not.
 *
 * @param sv The input value as a string_view
 * @return true if the string can be converted to a uint32; false otherwise
 */
[[nodiscard]] bool
checkIsU32Numeric(std::string_view sv);

/**
 * @brief Provides a commonly used validator for ledger index.
 *
 * LedgerIndex must be a string or an int. If the specified LedgerIndex is a string, its value must be either
 * "validated" or a valid integer value represented as a string.
 */
extern CustomValidator LedgerIndexValidator;

/**
 * @brief Provides a commonly used validator for accounts.
 *
 * Account must be a string and the converted public key is valid.
 */
extern CustomValidator AccountValidator;

/**
 * @brief Provides a commonly used validator for accounts.
 *
 * Account must be a string and can convert to base58.
 */
extern CustomValidator AccountBase58Validator;

/**
 * @brief Provides a commonly used validator for markers.
 *
 * A marker is composed of a comma-separated index and a start hint.
 * The former will be read as hex, and the latter can be cast to uint64.
 */
extern CustomValidator AccountMarkerValidator;

/**
 * @brief Provides a commonly used validator for uint256 hex string.
 *
 * It must be a string and also a decodable hex.
 * Transaction index, ledger hash all use this validator.
 */
extern CustomValidator Uint256HexStringValidator;

/**
 * @brief Provides a commonly used validator for currency, including standard currency code and token code.
 */
extern CustomValidator CurrencyValidator;

/**
 * @brief Provides a commonly used validator for issuer type.
 *
 * It must be a hex string or base58 string.
 */
extern CustomValidator IssuerValidator;

/**
 * @brief Provides a validator for validating streams used in subscribe/unsubscribe.
 */
extern CustomValidator SubscribeStreamValidator;

/**
 * @brief Provides a validator for validating accounts used in subscribe/unsubscribe.
 */
extern CustomValidator SubscribeAccountsValidator;

/**
 * @brief Validates an asset (ripple::Issue).
 *
 * Used by amm_info.
 */
extern CustomValidator CurrencyIssueValidator;

}  // namespace rpc::validation
