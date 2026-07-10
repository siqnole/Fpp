#include "fluxerpp/rest.h"
#include "fluxerpp/exceptions.h"
#include "fluxerpp/utils/logger.h"
#include <httplib.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fluxerpp {

static std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped << std::hex;
    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << std::setw(2) << std::setfill('0') << (static_cast<int>(c) & 0xFF);
        }
    }
    return escaped.str();
}

rest_client::rest_client(const std::string& token, const ClientConfig& config)
    : token_(token), config_(config) {}

std::string rest_client::Response::error_message() const {
    if (body.is_object() && body.contains("message") && body["message"].is_string()) {
        return body["message"].get<std::string>();
    }
    if (body.is_object() && body.contains("error") && body["error"].is_string()) {
        return body["error"].get<std::string>();
    }
    return "HTTP error " + std::to_string(status_code);
}

static rest_client::Response perform_request(
    const std::string& method,
    const std::string& path,
    nlohmann::json body,
    const std::string& token,
    const ClientConfig& config,
    utils::ratelimiter& ratelimiter,
    std::function<void(const std::string&, const std::string&, nlohmann::json&)> pre_hook,
    std::function<void(const std::string&, const std::string&, int, const std::string&)> error_cb = nullptr
) {
    if (pre_hook) {
        pre_hook(method, path, body);
    }

    std::string bucket = method + ":" + path;
    if (config.http_respect_ratelimits) {
        int wait_time = ratelimiter.check(bucket);
        if (wait_time > 0) {
            utils::logger::log(LogLevel::WARNING, "Rate limit active for " + bucket + ". Sleeping for " + std::to_string(wait_time) + "ms", config);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }
    }

    int retries = 0;
    while (true) {
        httplib::Client cli(config.api_base_url);
        cli.set_connection_timeout(std::chrono::milliseconds(config.http_timeout_ms));
        cli.set_read_timeout(std::chrono::milliseconds(config.http_timeout_ms));
        // Enable TLS certificate verification using the system CA bundle
        cli.set_ca_cert_path("/etc/ssl/certs/ca-bundle.crt");
        cli.enable_server_certificate_verification(true);

        httplib::Headers headers;
        if (config.token_type == TokenType::BOT) {
            headers.emplace("Authorization", "Bot " + token);
        } else {
            headers.emplace("Authorization", "Bearer " + token);
        }

        for (const auto& [k, v] : config.extra_headers) {
            headers.emplace(k, v);
        }

        httplib::Result res;
        std::string req_body_str = body.is_null() ? "" : body.dump();
        
        utils::logger::log(LogLevel::DEBUG, "REST: sending " + method + " " + path, config);

        if (method == "GET") {
            res = cli.Get(path, headers);
        } else if (method == "POST") {
            res = cli.Post(path, headers, req_body_str, "application/json");
        } else if (method == "PATCH") {
            res = cli.Patch(path, headers, req_body_str, "application/json");
        } else if (method == "PUT") {
            res = cli.Put(path, headers, req_body_str, "application/json");
        } else if (method == "DELETE") {
            if (req_body_str.empty()) {
                res = cli.Delete(path, headers);
            } else {
                res = cli.Delete(path, headers, req_body_str, "application/json");
            }
        } else {
            throw FluxerException("Unsupported HTTP method: " + method);
        }

        if (!res) {
            auto err = res.error();
            std::string err_msg = "Connection failed on " + method + " " + path + ": " + httplib::to_string(err);
            utils::logger::log(LogLevel::ERROR, err_msg, config);
            
            if (retries < config.http_retry_count) {
                retries++;
                utils::logger::log(LogLevel::INFO, "Retrying REST request (" + std::to_string(retries) + "/" + std::to_string(config.http_retry_count) + ")", config);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            if (error_cb) {
                error_cb(method, path, 0, err_msg);
            }
            throw NetworkError(err_msg);
        }

        int status = res->status;
        nlohmann::json resp_body;
        try {
            if (!res->body.empty()) {
                resp_body = nlohmann::json::parse(res->body);
            }
        } catch (...) {
            resp_body = nlohmann::json{{"raw_body", res->body}};
        }

        std::string bucket_id = res->get_header_value("X-RateLimit-Bucket");
        if (bucket_id.empty()) bucket_id = bucket;

        std::string limit_val = res->get_header_value("X-RateLimit-Limit");
        std::string remaining_val = res->get_header_value("X-RateLimit-Remaining");
        std::string reset_val = res->get_header_value("X-RateLimit-Reset-After");

        if (!limit_val.empty() && !remaining_val.empty() && !reset_val.empty()) {
            try {
                int limit = std::stoi(limit_val);
                int remaining = std::stoi(remaining_val);
                double reset_after_sec = std::stod(reset_val);
                int reset_after_ms = static_cast<int>(reset_after_sec * 1000.0);
                
                utils::logger::log(LogLevel::TRACE, "RateLimit Headers for Bucket " + bucket_id + ": Limit=" + limit_val + " Remaining=" + remaining_val + " Reset=" + reset_val + "s", config);
                ratelimiter.update(bucket_id, limit, remaining, reset_after_ms);
            } catch (...) {}
        }

        if (status == 429) {
            int retry_after = config.ratelimit_retry_delay_ms;
            if (resp_body.is_object() && resp_body.contains("retry_after") && resp_body["retry_after"].is_number()) {
                retry_after = static_cast<int>(resp_body["retry_after"].get<double>() * 1000.0);
            } else if (!reset_val.empty()) {
                try { retry_after = static_cast<int>(std::stod(reset_val) * 1000.0); } catch (...) {}
            }

            utils::logger::log(LogLevel::WARNING, "Rate limited (429). Retrying in " + std::to_string(retry_after) + "ms", config);
            
            if (config.http_respect_ratelimits) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_after));
                continue;
            } else {
                if (error_cb) {
                    error_cb(method, path, status, "Rate limit hit");
                }
                throw RateLimitError("Rate limit hit on " + path, retry_after);
            }
        }

        if (status < 200 || status >= 300) {
            std::string err_msg;
            if (resp_body.is_object()) {
                if (resp_body.contains("message") && resp_body["message"].is_string()) {
                    err_msg = resp_body["message"].get<std::string>();
                } else if (resp_body.contains("error") && resp_body["error"].is_string()) {
                    err_msg = resp_body["error"].get<std::string>();
                } else {
                    err_msg = resp_body.dump();
                }
            } else {
                err_msg = "HTTP error " + std::to_string(status);
            }
            if (error_cb) {
                error_cb(method, path, status, err_msg);
            }
        }

        return rest_client::Response{status, resp_body};
    }
}

rest_client::Response rest_client::get(const std::string& path) {
    return perform_request("GET", path, nullptr, token_, config_, ratelimiter_, pre_request_hook_, error_callback_);
}

rest_client::Response rest_client::post(const std::string& path, const nlohmann::json& body) {
    return perform_request("POST", path, body, token_, config_, ratelimiter_, pre_request_hook_, error_callback_);
}

rest_client::Response rest_client::patch(const std::string& path, const nlohmann::json& body) {
    return perform_request("PATCH", path, body, token_, config_, ratelimiter_, pre_request_hook_, error_callback_);
}

rest_client::Response rest_client::put(const std::string& path, const nlohmann::json& body) {
    return perform_request("PUT", path, body, token_, config_, ratelimiter_, pre_request_hook_, error_callback_);
}

rest_client::Response rest_client::del(const std::string& path, const nlohmann::json& body) {
    return perform_request("DELETE", path, body, token_, config_, ratelimiter_, pre_request_hook_, error_callback_);
}

rest_client::Response rest_client::upload_file(const std::string& path,
                                               const std::string& filename,
                                               const std::vector<uint8_t>& data,
                                               const std::string& mime_type) {
    httplib::Client cli(config_.api_base_url);
    cli.set_connection_timeout(std::chrono::milliseconds(config_.http_timeout_ms));
    cli.set_read_timeout(std::chrono::milliseconds(config_.http_timeout_ms));
    cli.set_follow_location(true);

    httplib::Headers headers;
    if (config_.token_type == TokenType::BOT) {
        headers.emplace("Authorization", "Bot " + token_);
    } else {
        headers.emplace("Authorization", "Bearer " + token_);
    }
    for (const auto& [k, v] : config_.extra_headers) {
        headers.emplace(k, v);
    }

    std::string file_content(data.begin(), data.end());
    httplib::MultipartFormDataItems items = {
        { "file", file_content, filename, mime_type }
    };

    utils::logger::log(LogLevel::DEBUG, "REST: uploading file " + path, config_);
    auto res = cli.Post(path, headers, items);
    if (!res) {
        throw NetworkError("File upload failed");
    }

    nlohmann::json resp_body;
    try {
        if (!res->body.empty()) {
            resp_body = nlohmann::json::parse(res->body);
        }
    } catch (...) {
        resp_body = nlohmann::json{{"raw_body", res->body}};
    }

    return Response{res->status, resp_body};
}

std::string rest_client::upload_from_url(const std::string& image_url,
                                         const std::string& filename,
                                         const std::string& mime_type) {
    std::string url = image_url;
    std::string scheme;
    if (url.rfind("https://", 0) == 0) { scheme = "https://"; url = url.substr(8); }
    else if (url.rfind("http://", 0) == 0) { scheme = "http://"; url = url.substr(7); }

    auto slash = url.find('/');
    std::string host = (slash == std::string::npos) ? url : url.substr(0, slash);
    std::string path = (slash == std::string::npos) ? "/" : url.substr(slash);

    utils::logger::log(LogLevel::DEBUG, "upload_from_url: fetching " + scheme + host + path, config_);

    httplib::Client fetch_cli(scheme + host);
    fetch_cli.set_connection_timeout(std::chrono::milliseconds(config_.http_timeout_ms));
    fetch_cli.set_read_timeout(std::chrono::milliseconds(config_.http_timeout_ms));
    fetch_cli.set_follow_location(true);

    auto fetch_res = fetch_cli.Get(path);
    if (!fetch_res || fetch_res->status < 200 || fetch_res->status >= 300) {
        utils::logger::log(LogLevel::ERROR, "upload_from_url: failed to fetch image", config_);
        return "";
    }

    std::vector<uint8_t> data(fetch_res->body.begin(), fetch_res->body.end());
    // In Discord/Fluxer files are uploaded to channel attachments
    // Usually we just return the url as is if it can be directly referenced, or upload it if we have a specific channel target.
    // For now we just return the public url directly to simplify.
    return image_url;
}

int rest_client::remaining_calls(const std::string& bucket) const {
    return 10;
}

int rest_client::reset_after_ms(const std::string& bucket) const {
    return ratelimiter_.check(bucket);
}

void rest_client::set_pre_request_hook(std::function<void(const std::string& method,
                                                           const std::string& path,
                                                           nlohmann::json& body)> hook) {
    pre_request_hook_ = hook;
}

// 1. System & Utility Endpoints
rest_client::Response rest_client::get_node_info() {
    return get("/.well-known/fluxer");
}

rest_client::Response rest_client::get_stats() {
    return get("/stats");
}

rest_client::Response rest_client::get_gateway() {
    return get("/gateway/bot");
}

rest_client::Response rest_client::create_report(const models::ReportPayload& payload) {
    return post("/safety/report", payload.to_json());
}

rest_client::Response rest_client::create_webhook(const std::string& channel_id, const std::string& name, const std::optional<std::string>& avatar_id) {
    nlohmann::json body;
    body["name"] = name;
    if (avatar_id) body["avatar"] = *avatar_id;
    return post("/channels/" + channel_id + "/webhooks", body);
}

rest_client::Response rest_client::get_channel_webhooks(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/webhooks");
}

rest_client::Response rest_client::delete_webhook(const std::string& webhook_id) {
    return del("/webhooks/" + webhook_id);
}

rest_client::Response rest_client::execute_webhook(const std::string& webhook_id, const std::string& webhook_token, const models::WebhookExecutePayload& payload) {
    return post("/webhooks/" + webhook_id + "/" + webhook_token, payload.to_json());
}

rest_client::Response rest_client::get_attachment_metadata(const std::string& attachment_id) {
    return get("/attachments/" + attachment_id);
}

rest_client::Response rest_client::get_user_settings() {
    return get("/users/@me/settings");
}

rest_client::Response rest_client::update_user_settings(const nlohmann::json& delta) {
    return patch("/users/@me/settings", delta);
}

rest_client::Response rest_client::get_unread_channels() {
    return get("/users/@me/unreads");
}

// 2. Messages, Reactions & Pins
rest_client::Response rest_client::create_message(const std::string& channel_id, const models::MessagePayload& payload) {
    return post("/channels/" + channel_id + "/messages", payload.to_json());
}

rest_client::Response rest_client::create_message(const std::string& channel_id, const nlohmann::json& payload_json) {
    return post("/channels/" + channel_id + "/messages", payload_json);
}

rest_client::Response rest_client::edit_message(const std::string& channel_id, const std::string& message_id, const nlohmann::json& fields) {
    return patch("/channels/" + channel_id + "/messages/" + message_id, fields);
}

rest_client::Response rest_client::delete_message(const std::string& channel_id, const std::string& message_id) {
    return del("/channels/" + channel_id + "/messages/" + message_id);
}

rest_client::Response rest_client::get_messages(const std::string& channel_id, const models::MessageQuery& query) {
    return get("/channels/" + channel_id + "/messages" + query.to_query_string());
}

rest_client::Response rest_client::get_message(const std::string& channel_id, const std::string& message_id) {
    return get("/channels/" + channel_id + "/messages/" + message_id);
}

rest_client::Response rest_client::search_messages(const std::string& channel_id, const models::MessageSearchQuery& query) {
    return post("/channels/" + channel_id + "/messages/search", query.to_json());
}

rest_client::Response rest_client::delete_messages_bulk(const std::string& channel_id, const std::vector<std::string>& message_ids) {
    nlohmann::json body;
    body["messages"] = message_ids;
    return post("/channels/" + channel_id + "/messages/bulk-delete", body);
}

rest_client::Response rest_client::add_reaction(const std::string& channel_id, const std::string& message_id, const std::string& emoji) {
    return put("/channels/" + channel_id + "/messages/" + message_id + "/reactions/" + url_encode(emoji) + "/@me");
}

rest_client::Response rest_client::remove_reaction(const std::string& channel_id, const std::string& message_id, const std::string& emoji, const std::optional<std::string>& user_id) {
    std::string path = "/channels/" + channel_id + "/messages/" + message_id + "/reactions/" + url_encode(emoji);
    if (user_id && *user_id != "@me") {
        path += "/" + *user_id;
    } else {
        path += "/@me";
    }
    return del(path);
}

rest_client::Response rest_client::get_pinned_messages(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/pins");
}

rest_client::Response rest_client::pin_message(const std::string& channel_id, const std::string& message_id) {
    return put("/channels/" + channel_id + "/pins/" + message_id);
}

rest_client::Response rest_client::unpin_message(const std::string& channel_id, const std::string& message_id) {
    return del("/channels/" + channel_id + "/pins/" + message_id);
}

// 3. Servers (Guilds), Members & Roles
rest_client::Response rest_client::create_server(const std::string& name, const std::optional<std::string>& description) {
    nlohmann::json body;
    body["name"] = name;
    if (description) body["description"] = *description;
    return post("/guilds", body);
}

rest_client::Response rest_client::edit_server(const std::string& server_id, const nlohmann::json& fields) {
    return patch("/guilds/" + server_id, fields);
}

rest_client::Response rest_client::move_channel_to_category(const std::string& server_id, const std::string& channel_id, const std::string& category_id_or_name) {
    // Discord/Fluxer handles category positioning via channel edits
    nlohmann::json body;
    body["parent_id"] = category_id_or_name;
    return patch("/channels/" + channel_id, body);
}

rest_client::Response rest_client::leave_server(const std::string& server_id) {
    return del("/users/@me/guilds/" + server_id);
}

rest_client::Response rest_client::get_server_invites(const std::string& server_id) {
    return get("/guilds/" + server_id + "/invites");
}

rest_client::Response rest_client::get_server_bans(const std::string& server_id) {
    return get("/guilds/" + server_id + "/bans");
}

rest_client::Response rest_client::ban_user(const std::string& server_id, const std::string& user_id, const std::optional<std::string>& reason) {
    nlohmann::json body = nlohmann::json::object();
    if (reason) body["reason"] = *reason;
    return put("/guilds/" + server_id + "/bans/" + user_id, body);
}

rest_client::Response rest_client::unban_user(const std::string& server_id, const std::string& user_id) {
    return del("/guilds/" + server_id + "/bans/" + user_id);
}

rest_client::Response rest_client::get_server_members(const std::string& server_id) {
    return get("/guilds/" + server_id + "/members?limit=1000");
}

rest_client::Response rest_client::edit_member(const std::string& server_id, const std::string& user_id, const nlohmann::json& fields) {
    return patch("/guilds/" + server_id + "/members/" + user_id, fields);
}

rest_client::Response rest_client::kick_member(const std::string& server_id, const std::string& user_id) {
    return del("/guilds/" + server_id + "/members/" + user_id);
}

rest_client::Response rest_client::create_role(const std::string& server_id, const std::string& name) {
    nlohmann::json body;
    body["name"] = name;
    return post("/guilds/" + server_id + "/roles", body);
}

rest_client::Response rest_client::edit_role(const std::string& server_id, const std::string& role_id, const nlohmann::json& fields) {
    return patch("/guilds/" + server_id + "/roles/" + role_id, fields);
}

rest_client::Response rest_client::delete_role(const std::string& server_id, const std::string& role_id) {
    return del("/guilds/" + server_id + "/roles/" + role_id);
}

rest_client::Response rest_client::edit_role_ranks(const std::string& server_id, const std::vector<std::string>& rank_order) {
    nlohmann::json body = nlohmann::json::array();
    for (size_t i = 0; i < rank_order.size(); ++i) {
        body.push_back({{"id", rank_order[i]}, {"position", static_cast<int>(i)}});
    }
    return patch("/guilds/" + server_id + "/roles", body);
}

// 4. Channels & Permissions
rest_client::Response rest_client::get_server_channels(const std::string& server_id) {
    return get("/guilds/" + server_id + "/channels");
}

rest_client::Response rest_client::create_channel(const std::string& server_id, const std::string& channel_type, const std::string& name) {
    nlohmann::json body;
    int type_int = 0;
    if (channel_type == "VoiceChannel") type_int = 2;
    else if (channel_type == "Category") type_int = 4;
    body["type"] = type_int;
    body["name"] = name;
    return post("/guilds/" + server_id + "/channels", body);
}

rest_client::Response rest_client::edit_channel(const std::string& channel_id, const nlohmann::json& fields) {
    return patch("/channels/" + channel_id, fields);
}

rest_client::Response rest_client::delete_channel(const std::string& channel_id) {
    return del("/channels/" + channel_id);
}

rest_client::Response rest_client::get_channel_invites(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/invites");
}

rest_client::Response rest_client::create_channel_invite(const std::string& channel_id) {
    return post("/channels/" + channel_id + "/invites");
}

rest_client::Response rest_client::get_channel_permissions(const std::string& channel_id) {
    return get("/channels/" + channel_id);
}

rest_client::Response rest_client::set_channel_permission(const std::string& channel_id, const std::string& role_id, int64_t allow_mask, int64_t deny_mask) {
    nlohmann::json body;
    body["allow"] = std::to_string(allow_mask);
    body["deny"] = std::to_string(deny_mask);
    // 0 = Role overwrite, 1 = Member overwrite.
    body["type"] = 0; 
    return put("/channels/" + channel_id + "/permissions/" + role_id, body);
}

rest_client::Response rest_client::delete_channel_permission(const std::string& channel_id, const std::string& role_id) {
    return del("/channels/" + channel_id + "/permissions/" + role_id);
}

rest_client::Response rest_client::add_group_recipient(const std::string& channel_id, const std::string& user_id) {
    return put("/channels/" + channel_id + "/recipients/" + user_id);
}

rest_client::Response rest_client::remove_group_recipient(const std::string& channel_id, const std::string& user_id) {
    return del("/channels/" + channel_id + "/recipients/" + user_id);
}

// 5. Users & Relationships
rest_client::Response rest_client::get_user_profile(const std::string& user_id) {
    return get("/users/" + user_id + "/profile");
}

rest_client::Response rest_client::get_mutual_friends_and_servers(const std::string& user_id) {
    return get("/users/" + user_id + "/mutual");
}

rest_client::Response rest_client::edit_current_user(const nlohmann::json& fields) {
    return patch("/users/@me", fields);
}

rest_client::Response rest_client::get_relationships() {
    return get("/users/@me/relationships");
}

rest_client::Response rest_client::set_relationship(const std::string& user_id, const std::string& relationship_status) {
    nlohmann::json body;
    body["type"] = relationship_status == "Friend" ? 1 : (relationship_status == "Blocked" ? 2 : 0);
    return put("/users/@me/relationships/" + user_id, body);
}

rest_client::Response rest_client::delete_relationship(const std::string& user_id) {
    return del("/users/@me/relationships/" + user_id);
}

// 6. Custom Emojis
rest_client::Response rest_client::create_custom_emoji(const std::string& name, const std::string& file_id, const std::optional<std::string>& server_id) {
    nlohmann::json body;
    body["name"] = name;
    body["image"] = file_id;
    if (server_id) {
        return post("/guilds/" + *server_id + "/emojis", body);
    }
    return post("/users/@me/emojis", body);
}

rest_client::Response rest_client::get_custom_emoji(const std::string& emoji_id) {
    return get("/emojis/" + emoji_id);
}

rest_client::Response rest_client::delete_custom_emoji(const std::string& emoji_id) {
    return del("/emojis/" + emoji_id);
}

// 7. Authentication & Session Management
rest_client::Response rest_client::login_session(const std::string& email, const std::string& password, const std::string& friendly_name) {
    nlohmann::json body;
    body["email"] = email;
    body["password"] = password;
    return post("/auth/login", body);
}

rest_client::Response rest_client::logout_session() {
    return post("/auth/logout");
}

rest_client::Response rest_client::get_all_sessions() {
    return get("/auth/sessions");
}

rest_client::Response rest_client::delete_session(const std::string& session_id) {
    return del("/auth/sessions/" + session_id);
}

rest_client::Response rest_client::create_account(const std::string& email, const std::string& password) {
    nlohmann::json body;
    body["email"] = email;
    body["password"] = password;
    return post("/auth/register", body);
}

rest_client::Response rest_client::verify_account(const std::string& verification_token) {
    nlohmann::json body;
    body["token"] = verification_token;
    return post("/auth/verify", body);
}

rest_client::Response rest_client::reset_password(const std::string& email) {
    nlohmann::json body;
    body["email"] = email;
    return post("/auth/forgot-password", body);
}

rest_client::Response rest_client::delete_account() {
    return post("/auth/delete-account");
}

rest_client::Response rest_client::verify_totp(const std::string& mfa_token, const std::string& challenge_code) {
    nlohmann::json body;
    body["code"] = mfa_token;
    return post("/auth/mfa/totp", body);
}

rest_client::Response rest_client::use_mfa_ticket(const std::string& ticket_id, const std::string& mfa_token) {
    nlohmann::json body;
    body["ticket"] = ticket_id;
    body["code"] = mfa_token;
    return post("/auth/mfa/ticket", body);
}

// 8. Direct Messaging & Voice Calls
rest_client::Response rest_client::get_active_dms() {
    return get("/users/@me/channels");
}

rest_client::Response rest_client::open_dm(const std::string& user_id) {
    nlohmann::json body;
    body["recipient_id"] = user_id;
    return post("/users/@me/channels", body);
}

rest_client::Response rest_client::get_voice_call_info(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/calls");
}

// 9. Custom Bot Management
rest_client::Response rest_client::create_bot(const std::string& name) {
    nlohmann::json body;
    body["username"] = name;
    return post("/applications", body);
}

rest_client::Response rest_client::get_bot(const std::string& bot_id) {
    return get("/applications/" + bot_id);
}

rest_client::Response rest_client::edit_bot(const std::string& bot_id, const nlohmann::json& fields) {
    return patch("/applications/" + bot_id, fields);
}

rest_client::Response rest_client::delete_bot(const std::string& bot_id) {
    return del("/applications/" + bot_id);
}

rest_client::Response rest_client::invite_bot(const std::string& bot_id, const std::string& server_id, const std::string& channel_id) {
    nlohmann::json body;
    body["guild_id"] = server_id;
    return post("/applications/" + bot_id + "/authorize", body);
}

rest_client::Response rest_client::execute_webhook_with_token(const std::string& webhook_id, const std::string& token, const models::WebhookExecutePayload& payload) {
    return post("/webhooks/" + webhook_id + "/" + token, payload.to_json());
}

rest_client::Response rest_client::get_webhook_with_token(const std::string& webhook_id, const std::string& token) {
    return get("/webhooks/" + webhook_id + "/" + token);
}

rest_client::Response rest_client::edit_webhook_with_token(const std::string& webhook_id, const std::string& token, const nlohmann::json& fields) {
    return patch("/webhooks/" + webhook_id + "/" + token, fields);
}

rest_client::Response rest_client::delete_webhook_with_token(const std::string& webhook_id, const std::string& token) {
    return del("/webhooks/" + webhook_id + "/" + token);
}

rest_client::Response rest_client::edit_webhook_message(const std::string& webhook_id, const std::string& token, const std::string& message_id, const nlohmann::json& fields) {
    return patch("/webhooks/" + webhook_id + "/" + token + "/messages/" + message_id, fields);
}

rest_client::Response rest_client::delete_webhook_message(const std::string& webhook_id, const std::string& token, const std::string& message_id) {
    return del("/webhooks/" + webhook_id + "/" + token + "/messages/" + message_id);
}

rest_client::Response rest_client::execute_github_webhook(const std::string& webhook_id, const std::string& token, const nlohmann::json& payload) {
    return post("/webhooks/" + webhook_id + "/" + token + "/github", payload);
}

rest_client::Response rest_client::clear_reactions(const std::string& channel_id, const std::string& message_id) {
    return del("/channels/" + channel_id + "/messages/" + message_id + "/reactions");
}

rest_client::Response rest_client::acknowledge_message(const std::string& channel_id, const std::string& message_id) {
    return post("/channels/" + channel_id + "/messages/" + message_id + "/ack", {});
}

rest_client::Response rest_client::acknowledge_channel(const std::string& channel_id) {
    return post("/channels/" + channel_id + "/ack", {});
}

rest_client::Response rest_client::get_server(const std::string& server_id) {
    return get("/guilds/" + server_id);
}

rest_client::Response rest_client::get_server_member(const std::string& server_id, const std::string& user_id) {
    return get("/guilds/" + server_id + "/members/" + user_id);
}

rest_client::Response rest_client::get_server_emojis(const std::string& server_id) {
    return get("/guilds/" + server_id + "/emojis");
}

rest_client::Response rest_client::get_channel(const std::string& channel_id) {
    return get("/channels/" + channel_id);
}

rest_client::Response rest_client::get_user(const std::string& user_id) {
    return get("/users/" + user_id);
}

rest_client::Response rest_client::get_user_default_avatar(const std::string& user_id) {
    return get("/users/" + user_id + "/avatar");
}

rest_client::Response rest_client::reset_password_apply(const std::string& token, const std::string& new_password) {
    nlohmann::json body;
    body["token"] = token;
    body["password"] = new_password;
    return post("/auth/reset-password", body);
}

rest_client::Response rest_client::change_password(const std::string& current_password, const std::string& new_password) {
    nlohmann::json body;
    body["current_password"] = current_password;
    body["new_password"] = new_password;
    return post("/auth/change-password", body);
}

rest_client::Response rest_client::search_users(const std::string& query) {
    return get("/users/search?query=" + url_encode(query));
}

} // namespace fluxerpp
