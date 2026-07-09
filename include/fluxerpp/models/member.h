#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace fluxerpp::models {

struct Member {
    std::string id;                                // user ID
    std::string server_id;                         // guild ID
    std::optional<std::string> nickname;
    std::optional<std::string> avatar;
    std::vector<std::string> roles;
    std::string joined_at;
    std::optional<std::string> timeout;            // ISO/date-time timestamp representing when the member's timeout ends
    nlohmann::json raw;                            // original parsed object

    static Member from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Member Member::from_json(const nlohmann::json& j) {
    Member m;
    m.raw = j;
    
    if (j.contains("user") && j["user"].is_object()) {
        if (j["user"].contains("id") && j["user"]["id"].is_string()) {
            m.id = j["user"]["id"].get<std::string>();
        }
    } else if (j.contains("user_id") && j["user_id"].is_string()) {
        m.id = j["user_id"].get<std::string>();
    } else if (j.contains("id") && j["id"].is_string()) {
        m.id = j["id"].get<std::string>();
    }
    
    if (j.contains("guild_id") && j["guild_id"].is_string()) m.server_id = j["guild_id"].get<std::string>();
    else if (j.contains("server_id") && j["server_id"].is_string()) m.server_id = j["server_id"].get<std::string>();
    
    if (j.contains("nick") && j["nick"].is_string()) m.nickname = j["nick"].get<std::string>();
    else if (j.contains("nickname") && j["nickname"].is_string()) m.nickname = j["nickname"].get<std::string>();
    
    if (j.contains("avatar") && j["avatar"].is_string()) m.avatar = j["avatar"].get<std::string>();
    
    if (j.contains("roles") && j["roles"].is_array()) {
        for (const auto& r : j["roles"]) {
            if (r.is_string()) {
                m.roles.push_back(r.get<std::string>());
            }
        }
    }
    
    if (j.contains("joined_at") && j["joined_at"].is_string()) {
        m.joined_at = j["joined_at"].get<std::string>();
    }
    
    if (j.contains("communication_disabled_until") && j["communication_disabled_until"].is_string()) {
        m.timeout = j["communication_disabled_until"].get<std::string>();
    } else if (j.contains("timeout") && j["timeout"].is_string()) {
        m.timeout = j["timeout"].get<std::string>();
    }
    return m;
}

inline nlohmann::json Member::to_json() const {
    nlohmann::json j = raw;
    j["user_id"] = id;
    j["guild_id"] = server_id;
    if (nickname) j["nick"] = *nickname;
    if (avatar) j["avatar"] = *avatar;
    j["roles"] = roles;
    if (!joined_at.empty()) j["joined_at"] = joined_at;
    if (timeout) j["communication_disabled_until"] = *timeout;
    return j;
}

} // namespace fluxerpp::models
