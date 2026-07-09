#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace fluxerpp {

class IHttpClient;
class cluster;
namespace events {
    struct Message;
}
namespace models {
    struct MessagePayload;
}

enum class LogLevel {
    NONE = 0,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    TRACE
};

enum class TokenType {
    BOT,   // sends "Authorization: Bot <token>" header
    USER   // sends "Authorization: <token>" or similar session header
};

struct ClientConfig {
    // --- Authentication ---
    TokenType token_type = TokenType::BOT;

    // --- Endpoints (override for self-hosted instances) ---
    std::string api_base_url   = "https://api.fluxer.app/v1";
    std::string ws_url         = "wss://gateway.fluxer.app";
    int         ws_version     = 1;
    std::string ws_format      = "json";

    // --- WebSocket behaviour ---
    int  ws_ping_interval_ms   = 41250;              // Discord heartbeats are usually ~41.25s, we can auto-detect it anyway but keep a default
    int  ws_reconnect_delay_ms = 5000;               // delay before reconnect
    bool ws_auto_reconnect     = true;
    bool ws_auth_in_url        = false;              // Auth via Identify gateway event by default

    // --- Ready payload fields ---
    std::vector<std::string> ready_fields = {};

    // --- REST ---
    int  http_timeout_ms       = 10000;
    int  http_retry_count      = 3;
    bool http_respect_ratelimits = true;
    int  ratelimit_retry_delay_ms = 500;             // wait before retry on 429

    // --- Logging ---
    LogLevel log_level         = LogLevel::INFO;
    // Custom log callback: override the built-in logger entirely
    std::function<void(LogLevel, const std::string&)> log_callback = nullptr;

    // --- Threading ---
    int event_thread_count     = 1;                  // threads for dispatching events
    int rest_thread_count      = 2;                  // threads for HTTP requests

    // --- Cache ---
    bool cache_messages        = false;              // off by default
    int  message_cache_size    = 1000;               // max messages per channel cached
    bool cache_members         = true;
    bool cache_channels        = true;
    bool cache_servers         = true;

    // --- Custom HTTP headers ---
    std::map<std::string, std::string> extra_headers = {};

    // --- Command prefix (like discord.py) ---
    std::string command_prefix = "!";
    // Custom Prefix Resolver: dynamically returns a list of prefixes for a message.
    std::function<std::vector<std::string>(class cluster&, const struct events::Message&)> prefix_resolver = nullptr;

    // --- Extension Hooks ---
    std::function<std::unique_ptr<IHttpClient>()> http_client_factory = nullptr;

    // --- Built-in Help Command ---
    bool enable_default_help = true;
    std::string default_help_color = "#5865f2";

    // --- Bot ownership ---
    std::string owner_id = "";

    // --- Command Handling ---
    bool dispatch_commands_on_edit = false; // Run commands when command messages are edited/updated
    bool case_insensitive_commands = false; // Match registered commands case-insensitively

    // --- Masquerade Override ---
    std::function<void(class cluster&, const std::string&, models::MessagePayload&)> masquerade_handler = nullptr;

    // --- Compatibility ---
    std::string cdn_url = "https://cdn.fluxer.app";
    std::string autumn_url = "https://cdn.fluxer.app";
};

} // namespace fluxerpp
