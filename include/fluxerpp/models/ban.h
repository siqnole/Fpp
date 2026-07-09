#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace fluxerpp::models {

struct Ban {
    std::string user_id;
    std::optional<std::string> reason;

    static Ban from_json(const nlohmann::json& j) {
        Ban b;
        if (j.contains("user") && j["user"].is_object()) {
            if (j["user"].contains("id") && j["user"]["id"].is_string()) {
                b.user_id = j["user"]["id"].get<std::string>();
            }
        } else if (j.contains("user_id") && j["user_id"].is_string()) {
            b.user_id = j["user_id"].get<std::string>();
        }
        if (j.contains("reason") && j["reason"].is_string()) b.reason = j["reason"].get<std::string>();
        return b;
    }
};

} // namespace fluxerpp::models
