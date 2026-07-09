#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace fluxerpp::models {

struct InstanceConfig {
    std::string stoat;                             // compatibility
    std::string version;
    
    struct Features {
        struct Captcha {
            bool enabled = false;
            std::string sitekey;
        };
        Captcha captcha;
        bool email = false;
        
        struct Autumn {
            bool enabled = false;
            std::string url;
        };
        Autumn autumn;
        
        struct January {
            bool enabled = false;
            std::string url;
        };
        January january;
    };
    Features features;
    std::string ws;

    static InstanceConfig from_json(const nlohmann::json& j) {
        InstanceConfig cfg;
        cfg.version = j.value("api_code_version", "1.0.0");
        cfg.stoat = "fluxer";
        
        if (j.contains("endpoints") && j["endpoints"].is_object()) {
            cfg.ws = j["endpoints"].value("gateway", "");
            cfg.features.autumn.url = j["endpoints"].value("media", "");
            cfg.features.autumn.enabled = !cfg.features.autumn.url.empty();
        }
        
        if (j.contains("captcha") && j["captcha"].is_object()) {
            cfg.features.captcha.enabled = j["captcha"].value("provider", "none") != "none";
            cfg.features.captcha.sitekey = j["captcha"].value("turnstile_site_key", j["captcha"].value("hcaptcha_site_key", ""));
        }
        
        if (j.contains("features") && j["features"].is_object()) {
            cfg.features.email = j["features"].value("emails_enabled", false);
        }
        
        return cfg;
    }
};

struct InstanceStats {
    int64_t users_count = 0;
    int64_t servers_count = 0;
    int64_t channels_count = 0;
    int64_t messages_count = 0;

    static InstanceStats from_json(const nlohmann::json& j) {
        InstanceStats stats;
        stats.users_count = j.value("users_count", int64_t(0));
        stats.servers_count = j.value("servers_count", int64_t(0));
        stats.channels_count = j.value("channels_count", int64_t(0));
        stats.messages_count = j.value("messages_count", int64_t(0));
        return stats;
    }
};

struct ReportPayload {
    std::string content_type; // "Message", "Server", "User"
    std::string id;
    std::string reason;
    std::optional<std::string> additional_context;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["content_type"] = content_type;
        j["id"] = id;
        j["reason"] = reason;
        if (additional_context) j["additional_context"] = *additional_context;
        return j;
    }
};

} // namespace fluxerpp::models
