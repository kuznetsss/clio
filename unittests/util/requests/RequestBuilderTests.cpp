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

#include "util/Expected.h"
#include "util/Fixtures.h"
#include "util/TestHttpServer.h"
#include "util/requests/RequestBuilder.h"
#include "util/requests/Types.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

using namespace util::requests;
using namespace boost;
using namespace boost::beast;

struct RequestBuilderTestBundle {
    std::string testName;
    http::verb method;
    std::vector<HttpHeader> headers;
    std::string target;
};

struct RequestBuilderTest : SyncAsioContextTest, testing::WithParamInterface<RequestBuilderTestBundle> {
    TestHttpServer server{ctx, "0.0.0.0", 11111};
    RequestBuilder builder{"localhost", "11111"};
};

INSTANTIATE_TEST_CASE_P(
    RequestBuilderTest,
    RequestBuilderTest,
    testing::Values(
        RequestBuilderTestBundle{"GetSimple", http::verb::get, {}, "/"},
        RequestBuilderTestBundle{
            "GetWithHeaders",
            http::verb::get,
            {{http::field::accept, "text/html"},
             {http::field::authorization, "password"},
             {"Custom_header", "some_value"}},
            "/"
        },
        RequestBuilderTestBundle{"GetWithTarget", http::verb::get, {}, "/test"},
        RequestBuilderTestBundle{"PostSimple", http::verb::post, {}, "/"},
        RequestBuilderTestBundle{
            "PostWithHeaders",
            http::verb::post,
            {{http::field::accept, "text/html"},
             {http::field::authorization, "password"},
             {"Custom_header", "some_value"}},
            "/"
        },
        RequestBuilderTestBundle{"PostWithTarget", http::verb::post, {}, "/test"}
    ),
    [](auto const& info) { return info.param.testName; }
);

TEST_P(RequestBuilderTest, SimpleRequest)
{
    std::string const replyBody = "Hello, world!";
    builder.addHeaders(GetParam().headers);
    builder.setTarget(GetParam().target);

    server.handleRequest(
        [&replyBody](http::request<http::string_body> request) -> std::optional<http::response<http::string_body>> {
            [&]() {
                EXPECT_TRUE(request.target() == GetParam().target);
                EXPECT_TRUE(request.method() == GetParam().method);
                for (auto const& header : GetParam().headers) {
                    std::visit(
                        [&](auto const& name) {
                            auto it = request.find(name);
                            ASSERT_NE(it, request.end());
                            EXPECT_EQ(it->value(), header.value);
                        },
                        header.name
                    );
                }
            }();
            return http::response<http::string_body>{http::status::ok, 11, replyBody};
        }
    );

    runSpawn([this, replyBody](asio::yield_context yield) {
        auto const response = [&]() -> util::Expected<std::string, RequestError> {
            switch (GetParam().method) {
                case http::verb::get:
                    return builder.get(yield);
                case http::verb::post:
                    return builder.post(yield);
                default:
                    return util::Unexpected{RequestError{"Invalid HTTP verb"}};
            }
        }();
        ASSERT_TRUE(response) << response.error().message;
        EXPECT_EQ(response.value(), replyBody);
    });
}

TEST_F(RequestBuilderTest, Timeout)
{
    builder.setTimeout(std::chrono::milliseconds{10});
    server.handleRequest(
        [](http::request<http::string_body> request) -> std::optional<http::response<http::string_body>> {
            [&]() {
                ASSERT_TRUE(request.target() == "/");
                ASSERT_TRUE(request.method() == http::verb::get);
            }();
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
            return std::nullopt;
        }
    );
    runSpawn([this](asio::yield_context yield) {
        auto response = builder.get(yield);
        EXPECT_FALSE(response);
    });
}

TEST_F(RequestBuilderTest, RequestWithBody)
{
    std::string const requestBody = "Hello, world!";
    std::string const replyBody = "Hello, client!";
    builder.addData(requestBody);

    server.handleRequest(
        [&](http::request<http::string_body> request) -> std::optional<http::response<http::string_body>> {
            [&]() {
                EXPECT_EQ(request.target(), "/");
                EXPECT_EQ(request.method(), http::verb::get);
                EXPECT_EQ(request.body(), requestBody);
            }();

            return http::response<http::string_body>{http::status::ok, 11, replyBody};
        }
    );

    runSpawn([&](asio::yield_context yield) {
        auto const response = builder.get(yield);
        ASSERT_TRUE(response) << response.error().message;
        EXPECT_EQ(response.value(), replyBody) << response.value();
    });
}

TEST_F(RequestBuilderTest, ResolveError)
{
    builder = RequestBuilder{"wrong_host", "11111"};
    runSpawn([this](asio::yield_context yield) {
        auto const response = builder.get(yield);
        ASSERT_FALSE(response);
        EXPECT_TRUE(response.error().message.starts_with("Resolve error")) << response.error().message;
    });
}

TEST_F(RequestBuilderTest, ConnectionError)
{
    builder = RequestBuilder{"localhost", "11112"};
    builder.setTimeout(std::chrono::milliseconds{1});
    runSpawn([this](asio::yield_context yield) {
        auto const response = builder.get(yield);
        ASSERT_FALSE(response);
        EXPECT_TRUE(response.error().message.starts_with("Connection error")) << response.error().message;
    });
}

TEST_F(RequestBuilderTest, WritingError)
{
    server.handleRequest(
        [](http::request<http::string_body> request) -> std::optional<http::response<http::string_body>> {
            [&]() {
                EXPECT_EQ(request.target(), "/");
                EXPECT_EQ(request.method(), http::verb::get);
            }();
            return std::nullopt;
        }
    );
}
