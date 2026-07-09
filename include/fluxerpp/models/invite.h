#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace fluxerpp::models {

struct Invite {
    std::string code;
    std::string channel_id;
    std::optional<std::string> server_id;
    std::string creator_id;

    static Invite from_json(const nlohmann::json& j) {
        Invite inv;
        if (j.contains("code") && j["code"].is_string()) inv.code = j["code"].get<std::string>();
        if (j.contains("channel_id") && j["channel_id"].is_string()) inv.channel_id = j["channel_id"].get<std::string>();
        if (j.contains("guild_id") && j["guild_id"].is_string()) inv.server_id = j["guild_id"].get<std::string>();
        else if (j.contains("server_id") && j["server_id"].is_string()) inv.server_id = j["server_id"].get<std::string>();
        if (j.contains("inviter") && j["inviter"].is_object()) {
            if (j["inviter"].contains("id") && j["inviter"]["id"].is_string()) {
                inv.creator_id = j["inviter"]["id"].get<std::string>();
            }
        }
        return inv;
    }
};

} // namespace fluxerpp::models
