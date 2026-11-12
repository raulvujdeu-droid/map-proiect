// Starter skeleton for the MAP project.
// Pure logic lives here so it can be unit tested without starting a server.
#pragma once

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

namespace map_project {

inline constexpr const char* kAppName = "map-project";
inline constexpr const char* kAppVersion = "0.1.0";
inline constexpr int kMaxUnfinishedTasks = 35;  // Parameter N = 35 from specification

/// Reads an environment variable, falling back when it is missing or empty.
inline std::string Env(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    return (value == nullptr || *value == '\0') ? fallback : std::string(value);
}

/// Returns the current UTC date formatted as YYYY-MM-DD.
inline std::string CurrentDateUtc() {
    const std::time_t now = std::time(nullptr);
    std::tm utc_tm{};
#if defined(_WIN32)
    gmtime_s(&utc_tm, &now);
#else
    gmtime_r(&now, &utc_tm);
#endif
    char buffer[11];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                  utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday);
    return std::string(buffer);
}

/// Validates task title: mandatory, 1 to 200 characters.
inline bool IsValidTitle(const std::string& title) {
    return !title.empty() && title.size() <= 200;
}

/// Validates task priority: integer between 1 and 5.
inline bool IsValidPriority(int priority) {
    return priority >= 1 && priority <= 5;
}

/// Validates calendar date in strict YYYY-MM-DD format.
inline bool IsValidDate(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }
    for (size_t i = 0; i < date.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(date[i]))) {
            return false;
        }
    }

    const int year = std::stoi(date.substr(0, 4));
    const int month = std::stoi(date.substr(5, 2));
    const int day = std::stoi(date.substr(8, 2));

    if (month < 1 || month > 12 || day < 1) {
        return false;
    }

    const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_day = days_in_month[month - 1];
    const bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (month == 2 && is_leap) {
        max_day = 29;
    }

    return day <= max_day;
}

/// Represents a single task item in memory.
struct Task {
    int id = 0;
    std::string title;
    int priority = 1;
    std::string due_date;
    bool completed = false;

    /// Derived property: overdue if unfinished and due_date is strictly before today.
    bool IsOverdue(const std::string& today) const {
        return !completed && due_date < today;
    }

    nlohmann::json ToJson(const std::string& today) const {
        return {
            {"id", id},
            {"title", title},
            {"priority", priority},
            {"due_date", due_date},
            {"completed", completed},
            {"overdue", IsOverdue(today)},
        };
    }
};

/// Holds the application data in memory.
/// Everything is lost when the container restarts.
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
<p>Autor: Vujdeu Raul, grupa 2.2</p>
<p>Tema: 2 - Lista de sarcini cu prioritati</p>
<p>Versiune: )" + kAppVersion + ", commit " + Env("APP_COMMIT", "dev") + R"(</p>
</body></html>)";
}

}  // namespace map_project