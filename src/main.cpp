// Wires the HTTP routes of the common contract and Tema 2 (Task Manager).

#include <chrono>
#include <iostream>
#include <optional>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "app.hpp"

namespace {

/// Helper to safely parse a positive integer from string.
std::optional<int> ParsePositiveInt(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }
    try {
        size_t pos = 0;
        int value = std::stoi(text, &pos);
        if (pos != text.size() || value <= 0) {
            return std::nullopt;
        }
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

void SendJson(httplib::Response& response, int status, const nlohmann::json& body) {
    response.status = status;
    response.set_content(body.dump(), "application/json; charset=utf-8");
}

}  // namespace

int main() {
    using namespace map_project;

    const auto started_at = std::chrono::steady_clock::now();
    Store store;
    httplib::Server server;

    // --- Common contract routes ---
    server.Get("/health", [&](const httplib::Request&, httplib::Response& response) {
        SendJson(response, 200, HealthBody(started_at));
    });

    server.Get("/version", [](const httplib::Request&, httplib::Response& response) {
        SendJson(response, 200, VersionBody());
    });

    server.Post("/reset", [&](const httplib::Request&, httplib::Response& response) {
        store.Reset();
        response.status = 204;
    });

    server.Get("/", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(HomePage(), "text/html; charset=utf-8");
    });

    // --- Tema 2 routes: Task Manager with Priorities ---

    // 1. Create a new task: POST /tasks
    server.Post("/tasks", [&](const httplib::Request& request, httplib::Response& response) {
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(request.body);
        } catch (...) {
            SendJson(response, 400, ErrorBody("validation", "invalid JSON body"));
            return;
        }

        if (!body.is_object() || !body.contains("title") || !body["title"].is_string() ||
            !body.contains("priority") || !body["priority"].is_number_integer() ||
            !body.contains("due_date") || !body["due_date"].is_string()) {
            SendJson(response, 400, ErrorBody("validation", "missing or invalid fields"));
            return;
        }

        const std::string title = body["title"].get<std::string>();
        const int priority = body["priority"].get<int>();
        const std::string due_date = body["due_date"].get<std::string>();

        if (!IsValidTitle(title)) {
            SendJson(response, 400, ErrorBody("validation", "title must be between 1 and 200 characters"));
            return;
        }
        if (!IsValidPriority(priority)) {
            SendJson(response, 400, ErrorBody("validation", "priority must be between 1 and 5"));
            return;
        }
        if (!IsValidDate(due_date)) {
            SendJson(response, 400, ErrorBody("validation", "due_date must be YYYY-MM-DD"));
            return;
        }

        OperationResult result = store.AddTask(title, priority, due_date);
        SendJson(response, result.status_code, result.body);
    });

    // 2. List tasks with optional filters: GET /tasks
    server.Get("/tasks", [&](const httplib::Request& request, httplib::Response& response) {
        std::optional<int> priority_filter;
        std::optional<std::string> due_before_filter;
        std::optional<bool> completed_filter;

        if (request.has_param("priority")) {
            auto parsed = ParsePositiveInt(request.get_param_value("priority"));
            if (!parsed.has_value() || !IsValidPriority(*parsed)) {
                SendJson(response, 400, ErrorBody("validation", "invalid priority filter"));
                return;
            }
            priority_filter = *parsed;
        }

        if (request.has_param("due_before")) {
            std::string due_before = request.get_param_value("due_before");
            if (!IsValidDate(due_before)) {
                SendJson(response, 400, ErrorBody("validation", "invalid due_before date"));
                return;
            }
            due_before_filter = due_before;
        }

        if (request.has_param("completed")) {
            std::string comp = request.get_param_value("completed");
            if (comp == "true") {
                completed_filter = true;
            } else if (comp == "false") {
                completed_filter = false;
            } else {
                SendJson(response, 400, ErrorBody("validation", "completed must be true or false"));
                return;
            }
        }

        SendJson(response, 200, store.ListTasks(priority_filter, due_before_filter, completed_filter));
    });

    // 3. Mark a task as done: PUT /tasks/{id}/done
    server.Put(R"(/tasks/(\d+)/done)", [&](const httplib::Request& request, httplib::Response& response) {
        auto id = ParsePositiveInt(request.matches[1]);
        if (!id.has_value()) {
            SendJson(response, 404, ErrorBody("not_found", "task not found"));
            return;
        }

        OperationResult result = store.MarkTaskDone(*id);
        SendJson(response, result.status_code, result.body);
    });

    // 4. Delete a task: DELETE /tasks/{id}
    server.Delete(R"(/tasks/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
        auto id = ParsePositiveInt(request.matches[1]);
        if (!id.has_value()) {
            SendJson(response, 404, ErrorBody("not_found", "task not found"));
            return;
        }

        OperationResult result = store.DeleteTask(*id);
        if (result.status_code == 204) {
            response.status = 204;
        } else {
            SendJson(response, result.status_code, result.body);
        }
    });

    // 5. Global statistics: GET /stats
    server.Get("/stats", [&](const httplib::Request&, httplib::Response& response) {
        SendJson(response, 200, store.GetStats());
    });

    server.set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if (response.status == 404) {
            SendJson(response, 404, ErrorBody("not_found", "route does not exist"));
        }
    });

    std::cout << kAppName << " " << kAppVersion << " starting on port 8080" << std::endl;
    if (!server.listen("0.0.0.0", 8080)) {
        std::cerr << "failed to bind port 8080" << std::endl;
        return 1;
    }
    return 0;
}