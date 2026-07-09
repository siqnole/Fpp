#include "fluxerpp/gateway.h"
#include "fluxerpp/rest.h"
#include "fluxerpp/utils/logger.h"
#include <ixwebsocket/IXWebSocket.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace fluxerpp {

struct gateway::impl {
    ix::WebSocket ws;
    std::thread ping_thread;
    std::atomic<bool> ping_thread_running{false};
    std::condition_variable ping_cv;
    std::mutex ping_mutex;
    std::atomic<bool> authenticated{false};
    std::atomic<int64_t> latency_ms{0};
    std::atomic<int64_t> last_sequence{0};
    std::atomic<int> heartbeat_interval_ms{41250};
    models::User self_user;
};

gateway::gateway(const std::string& token, const ClientConfig& config,
                 event_dispatcher& dispatcher)
    : token_(token), config_(config), dispatcher_(dispatcher), pimpl_(std::make_unique<impl>()) {
    
    pimpl_->ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            on_message_received(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            on_connected();
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            on_disconnected();
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::string err_msg = "WebSocket Error: " + msg->errorInfo.reason;
            utils::logger::log(LogLevel::ERROR, err_msg, config_);
        }
    });

    pimpl_->ws.enableAutomaticReconnection();
}

gateway::~gateway() {
    disconnect();
}

void gateway::connect() {
    try {
        rest_client rest(token_, config_);
        auto res = rest.get("/users/@me");
        if (res.success()) {
            pimpl_->self_user = models::User::from_json(res.body);
            utils::logger::log(LogLevel::INFO, "Gateway fetched self user: " + pimpl_->self_user.username + " (" + pimpl_->self_user.id + ")", config_);
        } else {
            utils::logger::log(LogLevel::ERROR, "Gateway failed to fetch self user: " + res.error_message(), config_);
        }
    } catch (const std::exception& e) {
        utils::logger::log(LogLevel::ERROR, "Gateway exception fetching self user: " + std::string(e.what()), config_);
    }

    std::string url = config_.ws_url;
    // Standard gateway URL logic
    utils::logger::log(LogLevel::INFO, "Connecting to Gateway: " + url, config_);
    pimpl_->ws.setUrl(url);
    pimpl_->ws.start();
}

void gateway::disconnect() {
    if (pimpl_->ping_thread_running) {
        pimpl_->ping_thread_running = false;
        pimpl_->ping_cv.notify_all();
        if (pimpl_->ping_thread.joinable()) {
            if (std::this_thread::get_id() == pimpl_->ping_thread.get_id()) {
                pimpl_->ping_thread.detach();
            } else {
                pimpl_->ping_thread.join();
            }
        }
    }

    if (is_connected()) {
        pimpl_->ws.stop();
    }
    pimpl_->authenticated = false;
}

bool gateway::is_connected() const {
    return pimpl_->ws.getReadyState() == ix::ReadyState::Open;
}

void gateway::send_raw(const nlohmann::json& payload) {
    if (!is_connected()) {
        utils::logger::log(LogLevel::WARNING, "Attempted to send payload when WebSocket is not connected: " + payload.dump(), config_);
        return;
    }
    utils::logger::log(LogLevel::TRACE, "WS Send: " + payload.dump(), config_);
    pimpl_->ws.send(payload.dump());
}

void gateway::authenticate(const std::string& token) {
    utils::logger::log(LogLevel::INFO, "Sending Identify event (Opcode 2)...", config_);
    std::string ws_token = (config_.token_type == TokenType::BOT) ? "Bot " + token : token;
    
    // Standard Discord Identify payload structure
    nlohmann::json properties = {
        {"os", "linux"},
        {"browser", "fluxerpp"},
        {"device", "fluxerpp"}
    };
    nlohmann::json identify_d = {
        {"token", ws_token},
        {"properties", properties},
        {"intents", 3276799} // all intents
    };
    
    send_raw(nlohmann::json{
        {"op", 2},
        {"d", identify_d}
    });
}

void gateway::ping(int64_t data) {
    int64_t seq = pimpl_->last_sequence.load();
    nlohmann::json payload = {
        {"op", 1}
    };
    if (seq > 0) {
        payload["d"] = seq;
    } else {
        payload["d"] = nullptr;
    }
    send_raw(payload);
}

void gateway::begin_typing(const std::string& channel_id) {
    // Discord typing trigger
    send_raw(nlohmann::json{
        {"op", 4},
        {"d", {{"channel_id", channel_id}}}
    });
}

void gateway::end_typing(const std::string& channel_id) {
    // No-op for Discord
}

void gateway::subscribe(const std::string& server_id) {
    // No-op for Discord
}

void gateway::on_message_received(const std::string& raw) {
    utils::logger::log(LogLevel::TRACE, "WS Recv: " + raw, config_);
    try {
        nlohmann::json j = nlohmann::json::parse(raw);
        handle_event(j);
    } catch (const std::exception& e) {
        utils::logger::log(LogLevel::ERROR, "Failed to parse WS payload: " + std::string(e.what()), config_);
    }
}

void gateway::on_connected() {
    utils::logger::log(LogLevel::INFO, "WebSocket connection established.", config_);
    // In Discord protocol, we wait for Hello (op: 10) before identifying
}

void gateway::on_disconnected() {
    utils::logger::log(LogLevel::INFO, "WebSocket connection closed.", config_);
    pimpl_->authenticated = false;
    
    if (pimpl_->ping_thread_running) {
        pimpl_->ping_thread_running = false;
        pimpl_->ping_cv.notify_all();
        if (pimpl_->ping_thread.joinable()) {
            if (std::this_thread::get_id() == pimpl_->ping_thread.get_id()) {
                pimpl_->ping_thread.detach();
            } else {
                pimpl_->ping_thread.join();
            }
        }
    }
    
    dispatcher_.dispatch_logout({});
}

void gateway::schedule_ping() {
    if (pimpl_->ping_thread_running) return;
    
    pimpl_->ping_thread_running = true;
    pimpl_->ping_thread = std::thread([this]() {
        utils::logger::log(LogLevel::DEBUG, "Starting Gateway heartbeat thread.", config_);
        while (pimpl_->ping_thread_running) {
            std::unique_lock<std::mutex> lock(pimpl_->ping_mutex);
            int interval = pimpl_->heartbeat_interval_ms.load();
            if (pimpl_->ping_cv.wait_for(lock, std::chrono::milliseconds(interval), [this]() {
                return !pimpl_->ping_thread_running;
            })) {
                break;
            }
            
            if (is_connected() && pimpl_->authenticated) {
                ping();
            }
        }
        utils::logger::log(LogLevel::DEBUG, "Gateway heartbeat thread exiting.", config_);
    });
}

void gateway::handle_event(const nlohmann::json& j) {
    if (!j.is_object() || !j.contains("op") || !j["op"].is_number()) return;
    
    int op = j["op"].get<int>();
    
    // Heartbeat ACK
    if (op == 11) {
        utils::logger::log(LogLevel::TRACE, "Heartbeat ACK received.", config_);
        return;
    }
    
    // Hello
    if (op == 10) {
        if (j.contains("d") && j["d"].is_object() && j["d"].contains("heartbeat_interval")) {
            pimpl_->heartbeat_interval_ms = j["d"]["heartbeat_interval"].get<int>();
            utils::logger::log(LogLevel::DEBUG, "Hello received. Heartbeat interval: " + std::to_string(pimpl_->heartbeat_interval_ms.load()) + "ms", config_);
        }
        authenticate(token_);
        return;
    }
    
    // Dispatch events
    if (op == 0) {
        if (j.contains("s") && j["s"].is_number()) {
            pimpl_->last_sequence = j["s"].get<int64_t>();
        }
        
        if (!j.contains("t") || !j["t"].is_string() || !j.contains("d")) return;
        
        std::string type = j["t"].get<std::string>();
        nlohmann::json d = j["d"];
        
        dispatcher_.dispatch_raw_event(type, d);
        
        if (type == "READY") {
            utils::logger::log(LogLevel::INFO, "Handshake completed. Authenticated successfully.", config_);
            pimpl_->authenticated = true;
            schedule_ping();
            
            events::Ready ev;
            if (d.contains("user")) {
                ev.user = models::User::from_json(d["user"]);
            } else {
                ev.user = pimpl_->self_user;
            }
            
            if (d.contains("guilds") && d["guilds"].is_array()) {
                for (const auto& g : d["guilds"]) {
                    ev.servers.push_back(models::Server::from_json(g));
                }
            }
            
            dispatcher_.dispatch_ready(ev);
            return;
        }
        
        if (type == "MESSAGE_CREATE") {
            events::Message ev = events::Message::from_json(d);
            dispatcher_.dispatch_message(ev);
            return;
        }
        
        if (type == "MESSAGE_UPDATE") {
            events::MessageUpdate ev;
            if (d.contains("id")) ev.id = d["id"].get<std::string>();
            if (d.contains("channel_id")) ev.channel_id = d["channel_id"].get<std::string>();
            ev.data = d;
            dispatcher_.dispatch_message_update(ev);
            return;
        }
        
        if (type == "MESSAGE_DELETE") {
            events::MessageDelete ev;
            if (d.contains("id")) ev.id = d["id"].get<std::string>();
            if (d.contains("channel_id")) ev.channel_id = d["channel_id"].get<std::string>();
            dispatcher_.dispatch_message_delete(ev);
            return;
        }
        
        if (type == "MESSAGE_REACTION_ADD") {
            events::MessageReact ev;
            if (d.contains("message_id")) ev.id = d["message_id"].get<std::string>();
            if (d.contains("channel_id")) ev.channel_id = d["channel_id"].get<std::string>();
            if (d.contains("user_id")) ev.user_id = d["user_id"].get<std::string>();
            if (d.contains("emoji") && d["emoji"].is_object() && d["emoji"].contains("name")) {
                ev.emoji_id = d["emoji"]["name"].get<std::string>();
            }
            dispatcher_.dispatch_message_react(ev);
            return;
        }
        
        if (type == "MESSAGE_REACTION_REMOVE") {
            events::MessageUnreact ev;
            if (d.contains("message_id")) ev.id = d["message_id"].get<std::string>();
            if (d.contains("channel_id")) ev.channel_id = d["channel_id"].get<std::string>();
            if (d.contains("user_id")) ev.user_id = d["user_id"].get<std::string>();
            if (d.contains("emoji") && d["emoji"].is_object() && d["emoji"].contains("name")) {
                ev.emoji_id = d["emoji"]["name"].get<std::string>();
            }
            dispatcher_.dispatch_message_unreact(ev);
            return;
        }
        
        if (type == "CHANNEL_CREATE") {
            events::ChannelCreate ev;
            ev.channel = models::Channel::from_json(d);
            ev.raw = d;
            dispatcher_.dispatch_channel_create(ev);
            return;
        }
        
        if (type == "CHANNEL_UPDATE") {
            events::ChannelUpdate ev;
            if (d.contains("id")) ev.id = d["id"].get<std::string>();
            ev.data = d;
            dispatcher_.dispatch_channel_update(ev);
            return;
        }
        
        if (type == "CHANNEL_DELETE") {
            events::ChannelDelete ev;
            if (d.contains("id")) ev.id = d["id"].get<std::string>();
            dispatcher_.dispatch_channel_delete(ev);
            return;
        }
        
        if (type == "GUILD_CREATE") {
            events::ServerCreate ev;
            ev.server = models::Server::from_json(d);
            ev.raw = d;
            dispatcher_.dispatch_server_create(ev);
            return;
        }
        
        if (type == "GUILD_UPDATE") {
            events::ServerUpdate ev;
            if (d.contains("id")) ev.id = d["id"].get<std::string>();
            ev.data = d;
            dispatcher_.dispatch_server_update(ev);
            return;
        }
        
        if (type == "GUILD_DELETE") {
            events::ServerDelete ev;
            if (d.contains("id")) ev.id = d["id"].get<std::string>();
            dispatcher_.dispatch_server_delete(ev);
            return;
        }
        
        if (type == "GUILD_MEMBER_ADD") {
            events::ServerMemberJoin ev;
            if (d.contains("guild_id")) ev.server_id = d["guild_id"].get<std::string>();
            if (d.contains("user") && d["user"].is_object() && d["user"].contains("id")) {
                ev.user_id = d["user"]["id"].get<std::string>();
            }
            ev.member = models::Member::from_json(d);
            dispatcher_.dispatch_server_member_join(ev);
            return;
        }
        
        if (type == "GUILD_MEMBER_REMOVE") {
            events::ServerMemberLeave ev;
            if (d.contains("guild_id")) ev.server_id = d["guild_id"].get<std::string>();
            if (d.contains("user") && d["user"].is_object() && d["user"].contains("id")) {
                ev.user_id = d["user"]["id"].get<std::string>();
            }
            dispatcher_.dispatch_server_member_leave(ev);
            return;
        }
        
        if (type == "GUILD_MEMBER_UPDATE") {
            events::ServerMemberUpdate ev;
            if (d.contains("guild_id")) ev.server_id = d["guild_id"].get<std::string>();
            if (d.contains("user") && d["user"].is_object() && d["user"].contains("id")) {
                ev.user_id = d["user"]["id"].get<std::string>();
            }
            ev.data = d;
            dispatcher_.dispatch_server_member_update(ev);
            return;
        }
        
        if (type == "GUILD_ROLE_CREATE") {
            events::ServerRoleCreate ev;
            if (d.contains("guild_id")) ev.server_id = d["guild_id"].get<std::string>();
            if (d.contains("role") && d["role"].is_object()) {
                ev.role = models::Role::from_json(d["role"]);
                ev.role_id = ev.role.id;
            }
            dispatcher_.dispatch_server_role_create(ev);
            return;
        }
        
        if (type == "GUILD_ROLE_UPDATE") {
            events::ServerRoleUpdate ev;
            if (d.contains("guild_id")) ev.server_id = d["guild_id"].get<std::string>();
            if (d.contains("role") && d["role"].is_object()) {
                if (d["role"].contains("id")) ev.role_id = d["role"]["id"].get<std::string>();
                ev.data = d["role"];
            }
            dispatcher_.dispatch_server_role_update(ev);
            return;
        }
        
        if (type == "GUILD_ROLE_DELETE") {
            events::ServerRoleDelete ev;
            if (d.contains("guild_id")) ev.server_id = d["guild_id"].get<std::string>();
            if (d.contains("role_id")) ev.role_id = d["role_id"].get<std::string>();
            dispatcher_.dispatch_server_role_delete(ev);
            return;
        }
        
        if (type == "TYPING_START") {
            events::ChannelStartTyping ev;
            if (d.contains("channel_id")) ev.channel_id = d["channel_id"].get<std::string>();
            if (d.contains("user_id")) ev.user_id = d["user_id"].get<std::string>();
            dispatcher_.dispatch_channel_start_typing(ev);
            return;
        }
    }
}

int64_t gateway::ping_latency() const {
    return pimpl_->latency_ms.load();
}

} // namespace fluxerpp
