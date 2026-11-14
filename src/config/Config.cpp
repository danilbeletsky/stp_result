#include "config/Config.h"

#include "domain/Text.h"

#include <array>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <map>

namespace pvz {

namespace {

constexpr std::array<const char*, 7> kKnownKeys = {
    "PVZ_DB_HOST",      "PVZ_DB_PORT",      "PVZ_DB_NAME",           "PVZ_DB_USER",
    "PVZ_DB_PASSWORD",  "PVZ_STORAGE_DAYS", "PVZ_CHECK_INTERVAL_SEC",
};

bool isKnownKey(const std::string& key) {
    for (const char* known : kKnownKeys) {
        if (key == known) {
            return true;
        }
    }
    return false;
}

std::map<std::string, std::string> readConfigFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw ConfigError("Не удалось открыть файл конфигурации: " + path.string());
    }

    std::map<std::string, std::string> values;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string trimmed = text::trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) {
            throw ConfigError(path.string() + ":" + std::to_string(lineNumber) +
                              ": ожидается строка вида KEY=VALUE");
        }
        const std::string key = text::trim(trimmed.substr(0, eq));
        if (!isKnownKey(key)) {
            throw ConfigError(path.string() + ":" + std::to_string(lineNumber) +
                              ": неизвестный параметр '" + key + "'");
        }
        values[key] = text::trim(trimmed.substr(eq + 1));
    }
    return values;
}

int parseIntInRange(const std::string& key, const std::string& value, int min, int max) {
    int result = 0;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc() || ptr != end) {
        throw ConfigError(key + ": ожидается целое число, получено '" + value + "'");
    }
    if (result < min || result > max) {
        throw ConfigError(key + ": значение должно быть от " + std::to_string(min) + " до " +
                          std::to_string(max));
    }
    return result;
}

} // namespace

EnvironmentLookup systemEnvironment() {
    return [](const std::string& name) -> std::optional<std::string> {
        if (const char* value = std::getenv(name.c_str())) {
            return std::string(value);
        }
        return std::nullopt;
    };
}

AppConfig loadConfig(const std::optional<std::filesystem::path>& configFile,
                     const EnvironmentLookup& environment) {
    std::map<std::string, std::string> values;
    if (configFile) {
        values = readConfigFile(*configFile);
    }
    for (const char* key : kKnownKeys) {
        if (auto value = environment(key)) {
            values[key] = *value;
        }
    }

    AppConfig config;
    auto apply = [&values](const char* key, auto&& setter) {
        if (auto it = values.find(key); it != values.end()) {
            setter(it->second);
        }
    };

    apply("PVZ_DB_HOST", [&](const std::string& v) { config.database.host = v; });
    apply("PVZ_DB_PORT", [&](const std::string& v) {
        config.database.port = parseIntInRange("PVZ_DB_PORT", v, 1, 65535);
    });
    apply("PVZ_DB_NAME", [&](const std::string& v) { config.database.name = v; });
    apply("PVZ_DB_USER", [&](const std::string& v) { config.database.user = v; });
    apply("PVZ_DB_PASSWORD", [&](const std::string& v) { config.database.password = v; });
    apply("PVZ_STORAGE_DAYS", [&](const std::string& v) {
        config.storageDays = parseIntInRange("PVZ_STORAGE_DAYS", v, 1, 365);
    });
    apply("PVZ_CHECK_INTERVAL_SEC", [&](const std::string& v) {
        config.checkIntervalSeconds = parseIntInRange("PVZ_CHECK_INTERVAL_SEC", v, 1, 86400);
    });

    if (config.database.host.empty()) {
        throw ConfigError("PVZ_DB_HOST не может быть пустым");
    }
    if (config.database.name.empty()) {
        throw ConfigError("PVZ_DB_NAME не может быть пустым");
    }
    return config;
}

} // namespace pvz
