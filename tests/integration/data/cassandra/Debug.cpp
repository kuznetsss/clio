#include "data/BackendFactory.hpp"
#include "data/BackendInterface.hpp"
#include "data/CassandraBackend.hpp"
#include "data/LedgerCache.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/LedgerData.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/Assert.hpp"
#include "util/MockPrometheus.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/Protocol.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

using util::config::ConfigType;
using util::config::ConfigValue;

struct DebugLedgerData : SyncAsioContextTest, util::prometheus::WithPrometheus {
    DebugLedgerData()
    {
        try {
            auto config = boost::json::parse(getConfig()).as_object();
            auto const configFileJson = util::config::ConfigFileJson{std::move(config)};
            if (auto errors = cfg.parse(configFileJson); errors.has_value()) {
                for (auto const& e : *errors) {
                    std::cerr << e.error << std::endl;
                }
                throw std::exception{};
            }
            backend = data::makeBackend(cfg, ledgerCache);
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            throw;
        }
    }

    static std::string
    getConfig()
    {
        static constexpr std::string_view kCONFIG_STR = R"json(
            {{
                "database": {{
                    "type": "cassandra",
                    "cassandra": {{
                        "contact_points": "{}",
                        "port": 9042,
                        "keyspace": "{}",
                        "username": "{}",
                        "password": "{}",
                        "threads": 16
                    }}
                }},
                "read_only": true
            }}
        )json";

        std::ifstream credentialsFile("credentials.json");
        ASSERT(credentialsFile.is_open(), "Couldn't open credentials.json");
        auto credentialsJson = boost::json::parse(credentialsFile).as_object();
        return fmt::format(
            kCONFIG_STR,
            std::string_view{credentialsJson["contact_points"].as_string()},
            std::string_view{credentialsJson["keyspace"].as_string()},
            std::string_view{credentialsJson["username"].as_string()},
            std::string_view{credentialsJson["password"].as_string()}
        );
    }

    util::config::ClioConfigDefinition cfg{
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points", ConfigValue{ConfigType::String}},
        {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.port", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
        {"database.cassandra.max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
        {"database.cassandra.threads",
         ConfigValue{ConfigType::Integer}.defaultValue(static_cast<uint32_t>(std::thread::hardware_concurrency()))},
        {"database.cassandra.core_connections_per_host", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.write_batch_size", ConfigValue{ConfigType::Integer}.defaultValue(20)},
        {"database.cassandra.connect_timeout", ConfigValue{ConfigType::Integer}.defaultValue(1).optional()},
        {"database.cassandra.request_timeout", ConfigValue{ConfigType::Integer}.defaultValue(1).optional()},
        {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.certfile", ConfigValue{ConfigType::String}.optional()},

        {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)}
    };
    data::LedgerCache ledgerCache{};

    std::shared_ptr<data::BackendInterface> backend;
    // ripple::LedgerIndex seq = 95474740;
    // || Keys size median: 79
    // || Keys size sum: 56380
    // || Keys size mean: 220

    ripple::LedgerIndex seq = 75474740;
    // || Keys size median: 41
    // || Keys size sum: 51682
    // || Keys size mean: 201
};

TEST_F(DebugLedgerData, Debug)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const start = std::chrono::steady_clock::now();
        auto page = backend->fetchLedgerPage(std::nullopt, seq, 256, false, yield);
        std::cout
            << "Total duration: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
            << " ms" << std::endl;
    });
    auto& keysSize = std::dynamic_pointer_cast<data::cassandra::CassandraBackend>(backend)->successor_keys_size;
    std::ranges::sort(keysSize);
    size_t sum = 0;
    std::ranges::for_each(keysSize, [&sum](size_t const k) { sum += k; });
    std::cout << "Keys size median: " << keysSize.at(keysSize.size() / 2) << std::endl;
    std::cout << "Keys size sum: " << sum << std::endl;
    std::cout << "Keys size mean: " << sum / keysSize.size() << std::endl;
    std::cout << "Keys size: " << keysSize.size() << std::endl;
}

TEST_F(DebugLedgerData, DebugHandler)
{
    rpc::LedgerDataHandler handler{backend};
    rpc::LedgerDataHandler::Input input{
        .ledgerHash = std::nullopt, .ledgerIndex = seq, .marker = std::nullopt, .diffMarker = std::nullopt
    };

    runSpawn([&](boost::asio::yield_context yield) {
        rpc::Context ctx{.yield = yield, .session = nullptr, .apiVersion = 1};
        auto const start = std::chrono::steady_clock::now();
        auto result = handler.process(std::move(input), ctx);
        std::cout
            << "Total duration: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
            << " ms" << std::endl;
    });
}
