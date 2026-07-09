#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace fluxerpp::models {

struct User {
    std::string id;
    std::string username;
    std::string discriminator;
    std::optional<std::string> global_name;
    std::optional<std::string> avatar;             // avatar hash
    std::optional<int> avatar_color;
    bool bot = false;
    bool system = false;
    int flags = 0;
    nlohmann::json raw;                            // original parsed object

    static User from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;

    std::string avatar_url(const std::string& cdn_url = "https://api.fluxer.app") const;
};

inline User User::from_json(const nlohmann::json& j) {
    User u;
    u.raw = j;
    if (j.contains("id") && j["id"].is_string()) u.id = j["id"].get<std::string>();
    
    if (j.contains("username") && j["username"].is_string()) u.username = j["username"].get<std::string>();
    if (j.contains("discriminator") && j["discriminator"].is_string()) u.discriminator = j["discriminator"].get<std::string>();
    if (j.contains("global_name") && j["global_name"].is_string()) u.global_name = j["global_name"].get<std::string>();
    
    if (j.contains("avatar") && j["avatar"].is_string()) u.avatar = j["avatar"].get<std::string>();
    if (j.contains("avatar_color") && j["avatar_color"].is_number()) u.avatar_color = j["avatar_color"].get<int>();
    
    if (j.contains("bot") && j["bot"].is_boolean()) u.bot = j["bot"].get<bool>();
    if (j.contains("system") && j["system"].is_boolean()) u.system = j["system"].get<bool>();
    if (j.contains("flags") && j["flags"].is_number()) u.flags = j["flags"].get<int>();
    return u;
}

inline nlohmann::json User::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["username"] = username;
    j["discriminator"] = discriminator;
    if (global_name) j["global_name"] = *global_name;
    if (avatar) j["avatar"] = *avatar;
    if (avatar_color) j["avatar_color"] = *avatar_color;
    j["bot"] = bot;
    j["system"] = system;
    j["flags"] = flags;
    return j;
}

inline std::string User::avatar_url(const std::string& cdn_url) const {
    if (avatar) {
        return cdn_url + "/assets/avatars/" + id + "/" + *avatar + ".png";
    }
    return cdn_url + "/assets/default_avatars/" + std::to_string((std::stoull(id) >> 22) % 5) + ".png";
}

} // namespace fluxerpp::models
