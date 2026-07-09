#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace fluxerpp::models {

struct Role {
    std::string id;
    std::string name;
    std::optional<std::string> colour;             // hex string
    int color = 0;                                 // raw color integer
    int rank = 0;                                  // Revolt style rank (mapped from position: rank = 1000 - position)
    int position = 0;
    nlohmann::json permissions;                    // allow/deny object or bitfield
    nlohmann::json raw;                            // original parsed object

    static Role from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;

    int64_t allowed_permissions() const {
        if (permissions.is_number()) {
            return permissions.get<int64_t>();
        } else if (permissions.is_string()) {
            try {
                return std::stoll(permissions.get<std::string>());
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }
};

inline Role Role::from_json(const nlohmann::json& j) {
    Role r;
    r.raw = j;
    if (j.contains("id") && j["id"].is_string()) r.id = j["id"].get<std::string>();
    if (j.contains("name") && j["name"].is_string()) r.name = j["name"].get<std::string>();
    
    if (j.contains("color") && j["color"].is_number()) {
        r.color = j["color"].get<int>();
        std::stringstream ss;
        ss << "#" << std::hex << std::setw(6) << std::setfill('0') << r.color;
        r.colour = ss.str();
    } else if (j.contains("colour") && j["colour"].is_string()) {
        r.colour = j["colour"].get<std::string>();
    }
    
    if (j.contains("position") && j["position"].is_number()) {
        r.position = j["position"].get<int>();
        r.rank = 1000 - r.position;
    } else if (j.contains("rank") && j["rank"].is_number()) {
        r.rank = j["rank"].get<int>();
        r.position = 1000 - r.rank;
    }
    
    if (j.contains("permissions")) {
        r.permissions = j["permissions"];
    }
    return r;
}

inline nlohmann::json Role::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["name"] = name;
    if (colour) j["colour"] = *colour;
    j["color"] = color;
    j["rank"] = rank;
    j["position"] = position;
    j["permissions"] = permissions;
    return j;
}

} // namespace fluxerpp::models
