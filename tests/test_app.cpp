#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <string>

#include "app.hpp"

using namespace map_project;

// --- Starter template tests (8 tests) ---

TEST_CASE("health body reports ok and uptime") {
    const auto started_at = std::chrono::steady_clock::now();
    const auto body = HealthBody(started_at);

    CHECK(body["status"] == "ok");
    CHECK(body.contains("uptime_seconds"));
    CHECK(body["uptime_seconds"].get<long>() >= 0);
}

TEST_CASE("version body contains all required fields") {
    const auto body = VersionBody();

    CHECK(body["app"] == kAppName);
    CHECK(body["version"] == kAppVersion);
    CHECK(body.contains("commit"));
    CHECK(body.contains("built_at"));
}

TEST_CASE("env falls back when the variable is missing") {
    CHECK(Env("MAP_VARIABLE_THAT_DOES_NOT_EXIST", "fallback") == "fallback");
}

TEST_CASE("error body has the standard shape") {
    const auto body = ErrorBody("not_found", "route does not exist");

    CHECK(body["error"] == "not_found");
    CHECK(body["message"] == "route does not exist");
}

TEST_CASE("new store starts at one") {
    Store store;
    CHECK(store.NextId() == 1);
}

TEST_CASE("take next id increments") {
    Store store;

    CHECK(store.TakeNextId() == 1);
    CHECK(store.TakeNextId() == 2);
    CHECK(store.NextId() == 3);
}

TEST_CASE("reset returns to one") {
    Store store;
    store.TakeNextId();
    store.TakeNextId();

    store.Reset();

    CHECK(store.NextId() == 1);
}

TEST_CASE("home page mentions the application name") {
    CHECK(HomePage().find(kAppName) != std::string::npos);
}

// --- Custom Tema 2 tests (12 tests) ---

TEST_CASE("validation: title length boundaries") {
    CHECK_FALSE(IsValidTitle(""));
    CHECK(IsValidTitle("A"));
    CHECK(IsValidTitle(std::string(200, 'x')));
    CHECK_FALSE(IsValidTitle(std::string(201, 'x')));
}

TEST_CASE("validation: priority range 1 to 5") {
    CHECK_FALSE(IsValidPriority(0));
    CHECK(IsValidPriority(1));
    CHECK(IsValidPriority(3));
    CHECK(IsValidPriority(5));
    CHECK_FALSE(IsValidPriority(6));
}

TEST_CASE("validation: strict YYYY-MM-DD date format") {
    CHECK(IsValidDate("2026-11-15"));
    CHECK(IsValidDate("2024-02-29"));   // leap year
    CHECK_FALSE(IsValidDate("2026-02-29"));  // non-leap year
    CHECK_FALSE(IsValidDate("2026-13-01"));  // invalid month
    CHECK_FALSE(IsValidDate("2026-11-31"));  // November has 30 days
    CHECK_FALSE(IsValidDate("15-11-2026"));  // wrong order
    CHECK_FALSE(IsValidDate("2026/11/15"));  // wrong separator
}

TEST_CASE("add task returns 201 and assigns sequential ids") {
    Store store;
    auto res1 = store.AddTask("First task", 3, "2099-12-01");
    auto res2 = store.AddTask("Second task", 5, "2099-12-05");

    CHECK(res1.status_code == 201);
    CHECK(res1.body["id"] == 1);
    CHECK(res1.body["title"] == "First task");
    CHECK(res1.body["completed"] == false);
    CHECK(res1.body["overdue"] == false);

    CHECK(res2.status_code == 201);
    CHECK(res2.body["id"] == 2);
}

TEST_CASE("list tasks sorts by priority desc, due_date asc, then id asc") {
    Store store;
    store.AddTask("Low priority early", 2, "2099-10-01");   // id 1
    store.AddTask("High priority late", 5, "2099-10-15");   // id 2
    store.AddTask("High priority early", 5, "2099-10-10");  // id 3
    store.AddTask("High priority tie", 5, "2099-10-10");    // id 4

    auto list = store.ListTasks(std::nullopt, std::nullopt, std::nullopt);
    REQUIRE(list.size() == 4);
    CHECK(list[0]["id"] == 3);  // priority 5, 2099-10-10, id 3
    CHECK(list[1]["id"] == 4);  // priority 5, 2099-10-10, id 4
    CHECK(list[2]["id"] == 2);  // priority 5, 2099-10-15, id 2
    CHECK(list[3]["id"] == 1);  // priority 2, 2099-10-01, id 1
}

TEST_CASE("list tasks supports priority, due_before, and completed filters") {
    Store store;
    store.AddTask("Task A", 4, "2099-05-01");
    store.AddTask("Task B", 4, "2099-06-01");
    store.AddTask("Task C", 2, "2099-04-01");
    store.MarkTaskDone(1);

    auto by_prio = store.ListTasks(4, std::nullopt, std::nullopt);
    CHECK(by_prio.size() == 2);

    auto by_date = store.ListTasks(std::nullopt, "2099-05-15", std::nullopt);
    CHECK(by_date.size() == 2);  // Task A and Task C

    auto only_done = store.ListTasks(std::nullopt, std::nullopt, true);
    REQUIRE(only_done.size() == 1);
    CHECK(only_done[0]["id"] == 1);
}

TEST_CASE("mark task done succeeds once and returns 409 on second attempt") {
    Store store;
    store.AddTask("Finish report", 4, "2099-11-20");

    auto first = store.MarkTaskDone(1);
    CHECK(first.status_code == 200);
    CHECK(first.body["completed"] == true);

    auto second = store.MarkTaskDone(1);
    CHECK(second.status_code == 409);
    CHECK(second.body["error"] == "conflict");
}

TEST_CASE("mark task done returns 404 for unknown id") {
    Store store;
    auto res = store.MarkTaskDone(999);
    CHECK(res.status_code == 404);
    CHECK(res.body["error"] == "not_found");
}

TEST_CASE("delete task removes item and returns 404 if missing") {
    Store store;
    store.AddTask("Temporary task", 1, "2099-12-31");

    auto del1 = store.DeleteTask(1);
    CHECK(del1.status_code == 204);

    auto del2 = store.DeleteTask(1);
    CHECK(del2.status_code == 404);
    CHECK(del2.body["error"] == "not_found");
}

TEST_CASE("enforce maximum 35 unfinished tasks limit and free slot on completion") {
    Store store;
    for (int i = 1; i <= 35; ++i) {
        auto res = store.AddTask("Task " + std::to_string(i), 3, "2099-12-31");
        CHECK(res.status_code == 201);
    }

    // 36th unfinished task must be rejected with 409
    auto overflow = store.AddTask("Overflow task", 3, "2099-12-31");
    CHECK(overflow.status_code == 409);
    CHECK(overflow.body["error"] == "conflict");

    // Completing one task frees a slot for a new one
    CHECK(store.MarkTaskDone(1).status_code == 200);
    auto after_done = store.AddTask("Allowed task", 3, "2099-12-31");
    CHECK(after_done.status_code == 201);
}

TEST_CASE("overdue flag is true only for unfinished tasks with past due_date") {
    Store store;
    store.AddTask("Old unfinished", 3, "2000-01-01");
    store.AddTask("Old finished", 3, "2000-01-01");
    store.AddTask("Future unfinished", 3, "2099-01-01");
    store.MarkTaskDone(2);

    auto list = store.ListTasks(std::nullopt, std::nullopt, std::nullopt);
    REQUIRE(list.size() == 3);

    for (const auto& item : list) {
        if (item["id"] == 1) CHECK(item["overdue"] == true);
        if (item["id"] == 2) CHECK(item["overdue"] == false);
        if (item["id"] == 3) CHECK(item["overdue"] == false);
    }
}

TEST_CASE("stats calculates totals, completed, overdue, and priority breakdown") {
    Store store;
    store.AddTask("T1", 5, "2000-01-01");  // overdue
    store.AddTask("T2", 5, "2099-01-01");  // completed
    store.AddTask("T3", 2, "2099-01-01");  // active
    store.MarkTaskDone(2);

    auto stats = store.GetStats();
    CHECK(stats["total"] == 3);
    CHECK(stats["completed"] == 1);
    CHECK(stats["overdue"] == 1);
    CHECK(stats["by_priority"]["5"] == 2);
    CHECK(stats["by_priority"]["2"] == 1);
    CHECK(stats["by_priority"]["1"] == 0);
}