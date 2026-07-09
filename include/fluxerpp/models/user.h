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

    // Compatibility fields
    std::optional<std::string> display_name;
    std::optional<std::string> presence;
    std::optional<std::string> status_text;
    std::optional<std::string> bio;
    std::optional<std::string> banner;

    static User from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;

    std::string avatar_url(const std::string& cdn_url = "https://api.fluxer.app") const;
    std::string banner_url(const std::string& cdn_url = "https://api.fluxer.app") const;
};

inline User User::from_json(const nlohmann::json& j) {
    User u;
    u.raw = j;
    if (j.contains("id") && j["id"].is_string()) u.id = j["id"].get<std::string>();
    
    if (j.contains("username") && j["username"].is_string()) u.username = j["username"].get<std::string>();
    if (j.contains("discriminator") && j["discriminator"].is_string()) u.discriminator = j["discriminator"].get<std::string>();
    if (j.contains("global_name") && j["global_name"].is_string()) {
        u.global_name = j["global_name"].get<std::string>();
    }
    
    if (j.contains("avatar") && j["avatar"].is_string()) u.avatar = j["avatar"].get<std::string>();
    if (j.contains("avatar_color") && j["avatar_color"].is_number()) u.avatar_color = j["avatar_color"].get<int>();
    
    if (j.contains("bot") && j["bot"].is_boolean()) u.bot = j["bot"].get<bool>();
    if (j.contains("system") && j["system"].is_boolean()) u.system = j["system"].get<bool>();
    if (j.contains("flags") && j["flags"].is_number()) u.flags = j["flags"].get<int>();

    // Parse compatibility fields
    if (u.global_name) {
        u.display_name = *u.global_name;
    } else if (j.contains("display_name") && j["display_name"].is_string()) {
        u.display_name = j["display_name"].get<std::string>();
    } else {
        u.display_name = u.username;
    }
    
    if (j.contains("bio") && j["bio"].is_string()) u.bio = j["bio"].get<std::string>();
    if (j.contains("banner") && j["banner"].is_string()) u.banner = j["banner"].get<std::string>();
    if (j.contains("presence") && j["presence"].is_string()) u.presence = j["presence"].get<std::string>();
    if (j.contains("status_text") && j["status_text"].is_string()) u.status_text = j["status_text"].get<std::string>();
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
    if (display_name) j["display_name"] = *display_name;
    if (bio) j["bio"] = *bio;
    if (banner) j["banner"] = *banner;
    if (presence) j["presence"] = *presence;
    if (status_text) j["status_text"] = *status_text;
    return j;
}

inline std::string User::avatar_url(const std::string& cdn_url) const {
    if (avatar) {
        return cdn_url + "/assets/avatars/" + id + "/" + *avatar + ".png";
    }
    return cdn_url + "/assets/default_avatars/" + std::to_string((std::stoull(id) >> 22) % 5) + ".png";
}

inline std::string User::banner_url(const std::string& cdn_url) const {
    if (banner) {
        return cdn_url + "/assets/banners/" + id + "/" + *banner + ".png";
    }
    return "";
}

} // namespace fluxerpp::models
