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

#include "rpc/Errors.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/protocol/ErrorCodes.h>

#include <exception>

using namespace rpc::validation;

TEST(NotSupportedTests, CheckField)
{
    NotSupported notSupported;

    boost::json::value const json{
        {"field1", "value1"},
        {"field2", 123},
    };
    auto const result = notSupported.verify(json, "field1");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, rpc::CombinedError{rpc::RippledError::rpcNOT_SUPPORTED});

    EXPECT_FALSE(notSupported.verify(json, "field2"));
    EXPECT_TRUE(notSupported.verify(json, "field3"));
}

TEST(NotSupportedTests, CheckValue)
{
    NotSupported notSupported{123};
    boost::json::value json{
        {"field1", "value1"},
        {"field2", 123},
    };
    EXPECT_THROW({ [[maybe_unused]] auto const r = notSupported.verify(json, "field1"); }, std::exception);

    auto const result = notSupported.verify(json, "field2");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, rpc::CombinedError{rpc::RippledError::rpcNOT_SUPPORTED});

    EXPECT_TRUE(notSupported.verify(json, "field3"));

    json.at("field2") = 456;
    EXPECT_TRUE(notSupported.verify(json, "field2"));
}

TEST(DeprecatedTests, CheckField)
{
    Deprecated deprecated;

    boost::json::value const json{
        {"field1", "value1"},
        {"field2", 123},
    };
    auto const result = deprecated.verify(json, "field1");
    ASSERT_FALSE(result);

    EXPECT_FALSE(deprecated.verify(json, "field2"));
    EXPECT_TRUE(deprecated.verify(json, "field3"));
}

TEST(DeprecatedTests, CheckValue)
{
    Deprecated deprecated{123};
    boost::json::value json{
        {"field1", "value1"},
        {"field2", 123},
    };
    EXPECT_THROW({ [[maybe_unused]] auto const r = deprecated.verify(json, "field1"); }, std::exception);

    auto const result = deprecated.verify(json, "field2");
    ASSERT_FALSE(result);

    EXPECT_TRUE(deprecated.verify(json, "field3"));

    json.at("field2") = 456;
    EXPECT_TRUE(deprecated.verify(json, "field2"));
}
