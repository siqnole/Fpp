#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace fluxerpp::models {

struct Channel {
    std::string id;
    int type = 0;                                  // Discord style channel type integer
    std::string channel_type;                      // "TextChannel", "DirectMessage", etc. (for Bronx compatibility)
    std::optional<std::string> server;             // Represents Guild / Community ID
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> icon;
    bool nsfw = false;
    std::optional<nlohmann::json> default_permissions;
    std::optional<nlohmann::json> role_permissions;
    nlohmann::json raw;                            // original parsed object

    static Channel from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Channel Channel::from_json(const nlohmann::json& j) {
    Channel c;
    c.raw = j;
    if (j.contains("id") && j["id"].is_string()) c.id = j["id"].get<std::string>();
    
    if (j.contains("type") && j["type"].is_number()) {
        c.type = j["type"].get<int>();
        if (c.type == 0) c.channel_type = "TextChannel";
        else if (c.type == 1) c.channel_type = "DirectMessage";
        else if (c.type == 2) c.channel_type = "VoiceChannel";
        else if (c.type == 3) c.channel_type = "GroupDM";
        else if (c.type == 4) c.channel_type = "Category";
        else c.channel_type = "Unknown";
    } else if (j.contains("channel_type") && j["channel_type"].is_string()) {
        c.channel_type = j["channel_type"].get<std::string>();
        if (c.channel_type == "TextChannel") c.type = 0;
        else if (c.channel_type == "DirectMessage") c.type = 1;
        else if (c.channel_type == "VoiceChannel") c.type = 2;
        else if (c.channel_type == "GroupDM") c.type = 3;
        else if (c.channel_type == "Category") c.type = 4;
    }
    
    if (j.contains("guild_id") && j["guild_id"].is_string()) c.server = j["guild_id"].get<std::string>();
    else if (j.contains("server") && j["server"].is_string()) c.server = j["server"].get<std::string>();
    
    if (j.contains("name") && j["name"].is_string()) c.name = j["name"].get<std::string>();
    if (j.contains("topic") && j["topic"].is_string()) c.description = j["topic"].get<std::string>();
    else if (j.contains("description") && j["description"].is_string()) c.description = j["description"].get<std::string>();
    
    if (j.contains("icon") && j["icon"].is_string()) c.icon = j["icon"].get<std::string>();
    if (j.contains("nsfw") && j["nsfw"].is_boolean()) c.nsfw = j["nsfw"].get<bool>();
    
    if (j.contains("permission_overwrites")) c.role_permissions = j["permission_overwrites"];
    else if (j.contains("role_permissions")) c.role_permissions = j["role_permissions"];
    
    if (j.contains("default_permissions")) c.default_permissions = j["default_permissions"];
    return c;
}

inline nlohmann::json Channel::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["type"] = type;
    j["channel_type"] = channel_type;
    if (server) j["guild_id"] = *server;
    if (name) j["name"] = *name;
    if (description) j["topic"] = *description;
    if (icon) j["icon"] = *icon;
    j["nsfw"] = nsfw;
    if (role_permissions) j["permission_overwrites"] = *role_permissions;
    if (default_permissions) j["default_permissions"] = *default_permissions;
    return j;
}

} // namespace fluxerpp::models
