#include "config/Config.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>

using namespace pvz;

namespace {

EnvironmentLookup fakeEnvironment(std::map<std::string, std::string> values) {
    return [values = std::move(values)](const std::string& name) -> std::optional<std::string> {
        if (auto it = values.find(name); it != values.end()) {
            return it->second;
        }
        return std::nullopt;
    };
}

std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path) << content;
    return path;
}

} // namespace

TEST(ConfigTest, UsesDefaultsWithoutFileAndEnvironment) {
    const AppConfig config = loadConfig(std::nullopt, fakeEnvironment({}));
    EXPECT_EQ(config.database.host, "localhost");
    EXPECT_EQ(config.database.port, 5432);
    EXPECT_EQ(config.database.name, "pvz");
    EXPECT_EQ(config.storageDays, 7);
}

TEST(ConfigTest, EnvironmentOverridesFile) {
    const auto file = writeTempFile("pvz_test.conf",
                                    "# комментарий\n"
                                    "PVZ_DB_HOST = db.local\n"
                                    "PVZ_DB_NAME=from_file\n"
                                    "\n"
                                    "PVZ_STORAGE_DAYS=3\n");
    const AppConfig config =
        loadConfig(file, fakeEnvironment({{"PVZ_DB_NAME", "from_env"}, {"PVZ_DB_PORT", "6543"}}));
    EXPECT_EQ(config.database.host, "db.local");
    EXPECT_EQ(config.database.name, "from_env");
    EXPECT_EQ(config.database.port, 6543);
    EXPECT_EQ(config.storageDays, 3);
    std::filesystem::remove(file);
}

TEST(ConfigTest, RejectsInvalidValues) {
    EXPECT_THROW(loadConfig(std::nullopt, fakeEnvironment({{"PVZ_DB_PORT", "abc"}})), ConfigError);
    EXPECT_THROW(loadConfig(std::nullopt, fakeEnvironment({{"PVZ_DB_PORT", "70000"}})), ConfigError);
    EXPECT_THROW(loadConfig(std::nullopt, fakeEnvironment({{"PVZ_STORAGE_DAYS", "0"}})), ConfigError);
    EXPECT_THROW(loadConfig(std::nullopt, fakeEnvironment({{"PVZ_DB_NAME", ""}})), ConfigError);
}

TEST(ConfigTest, RejectsBrokenFile) {
    EXPECT_THROW(loadConfig(std::filesystem::path("/nonexistent/pvz.conf"), fakeEnvironment({})),
                 ConfigError);
    const auto unknownKey = writeTempFile("pvz_bad1.conf", "PVZ_DB_HOTS=localhost\n");
    EXPECT_THROW(loadConfig(unknownKey, fakeEnvironment({})), ConfigError);
    const auto noEquals = writeTempFile("pvz_bad2.conf", "PVZ_DB_HOST localhost\n");
    EXPECT_THROW(loadConfig(noEquals, fakeEnvironment({})), ConfigError);
    std::filesystem::remove(unknownKey);
    std::filesystem::remove(noEquals);
}
