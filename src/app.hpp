// Starter skeleton for the MAP project.
// Pure logic lives here so it can be unit tested without starting a server.
#pragma once

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

namespace map_project {

inline constexpr const char* kAppName = "map-project";
inline constexpr const char* kAppVersion = "0.1.0";

/// Reads an environment variable, falling back when it is missing or empty.
inline std::string Env(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    return (value == nullptr || *value == '\0') ? fallback : std::string(value);
}

/// Holds the application data in memory.
/// Everything is lost when the container restarts. Add your theme's structures here.
class Store {
public:
    void Reset() {
        std::lock_guard<std::mutex> guard(mutex_);
        next_id_ = 1;
    }

    int NextId() {
        std::lock_guard<std::mutex> guard(mutex_);
        return next_id_;
    }

    int TakeNextId() {
        std::lock_guard<std::mutex> guard(mutex_);
        return next_id_++;
    }

private:
    std::mutex mutex_;
    int next_id_ = 1;
    // example: std::map<int, Contact> contacts_;
};

/// Seconds since the process started.
inline long UptimeSeconds(std::chrono::steady_clock::time_point started_at) {
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
}

inline nlohmann::json HealthBody(std::chrono::steady_clock::time_point started_at) {
    return {{"status", "ok"}, {"uptime_seconds", UptimeSeconds(started_at)}};
}

inline nlohmann::json VersionBody() {
    return {
        {"app", kAppName},
        {"version", kAppVersion},
        {"commit", Env("APP_COMMIT", "dev")},
        {"built_at", Env("APP_BUILT_AT", "unknown")},
    };
}

inline nlohmann::json ErrorBody(const std::string& code, const std::string& message) {
    return {{"error", code}, {"message", message}};
}

inline std::string HomePage() {
    return std::string(R"(<!DOCTYPE html>
<html lang="ro"><head><meta charset="utf-8"><title>)") + kAppName + R"(</title></head>
<body>
<h1>)" + kAppName + R"(</h1>
<p>Autor: NUME PRENUME, grupa GRUPA</p>
<p>Tema: NUMARUL TEMEI</p>
<p>Versiune: )" + kAppVersion + ", commit " + Env("APP_COMMIT", "dev") + R"(</p>
</body></html>)";
}

}  // namespace map_project
