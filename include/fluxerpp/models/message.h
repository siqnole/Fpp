#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace fluxerpp::models {

struct Embed {
    std::optional<std::string>   title;
    std::optional<std::string>   description;
    std::optional<std::string>   url;          // link on the title
    std::optional<std::string>   icon_url;     // small icon next to title
    std::optional<std::string>   colour;       // CSS colour string, e.g. "#5865f2"
    std::optional<std::string>   media;        // attachment URL or ID for embed image

    Embed& set_image(const std::string& image_url_or_id) {
        media = image_url_or_id;
        return *this;
    }

    Embed& set_title(const std::string& t) { title = t; return *this; }
    Embed& set_description(const std::string& d) { description = d; return *this; }
    Embed& set_url(const std::string& u) { url = u; return *this; }
    Embed& set_icon_url(const std::string& i) { icon_url = i; return *this; }
    Embed& set_colour(const std::string& c) { colour = c; return *this; }
    Embed& set_colour(uint32_t rgb) {
        char hex[8];
        snprintf(hex, sizeof(hex), "#%06x", rgb & 0xffffff);
        colour = hex;
        return *this;
    }

    nlohmann::json to_json() const {
        nlohmann::json j = nlohmann::json::object();
        if (title)       j["title"]       = *title;
        if (description) j["description"] = *description;
        if (url)         j["url"]         = *url;
        
        if (colour) {
            std::string c = *colour;
            if (!c.empty() && c[0] == '#') {
                try {
                    j["color"] = std::stoul(c.substr(1), nullptr, 16);
                } catch (...) {}
            }
        }
        
        if (media) {
            nlohmann::json img = nlohmann::json::object();
            img["url"] = *media;
            j["image"] = img;
        }
        
        if (icon_url) {
            nlohmann::json thumb = nlohmann::json::object();
            thumb["url"] = *icon_url;
            j["thumbnail"] = thumb;
        }
        
        return j;
    }
};

struct MessagePayload {
    std::string content;
    std::optional<std::string> nonce;

    struct Reply {
        std::string id;
        bool mention = false;
    };
    std::vector<Reply> replies;

    struct Masquerade {
        std::optional<std::string> name;
        std::optional<std::string> avatar;
        std::optional<std::string> colour;
    };
    std::optional<Masquerade> masquerade; // Note: compatibility field

    // Embeds (send custom embed objects)
    std::vector<nlohmann::json> embeds;

    // Attachments
    std::vector<std::string> attachments;

    // Interactions
    struct Interactions {
        std::vector<std::string> reactions;
        bool restrict_reactions = false;
    };
    std::optional<Interactions> interactions;

    // library-only: auto-delete this message after N seconds
    int delete_after = 0;

    MessagePayload& set_content(const std::string& c) { content = c; return *this; }
    MessagePayload& set_nonce(const std::string& n) { nonce = n; return *this; }
    MessagePayload& add_reply(const std::string& message_id, bool mention = false) {
        replies.push_back({message_id, mention});
        return *this;
    }
    MessagePayload& add_embed(const Embed& embed) {
        embeds.push_back(embed.to_json());
        return *this;
    }
    MessagePayload& add_attachment(const std::string& file_id) {
        attachments.push_back(file_id);
        return *this;
    }
    MessagePayload& set_delete_after(int seconds) {
        delete_after = seconds;
        return *this;
    }
    MessagePayload& set_interactions(const std::vector<std::string>& rx, bool restrict = false) {
        interactions = Interactions{rx, restrict};
        return *this;
    }

    nlohmann::json to_json() const {
        nlohmann::json j = nlohmann::json::object();
        j["content"] = content;
        if (nonce) j["nonce"] = *nonce;
        
        if (!replies.empty()) {
            // Discord uses message_reference
            nlohmann::json ref = nlohmann::json::object();
            ref["message_id"] = replies[0].id;
            j["message_reference"] = ref;
            // Discord also has allowed_mentions
            nlohmann::json mentions = nlohmann::json::object();
            mentions["replied_user"] = replies[0].mention;
            j["allowed_mentions"] = mentions;
        }
        
        if (!embeds.empty()) {
            j["embeds"] = embeds;
        }
        
        // Attachment objects
        if (!attachments.empty()) {
            nlohmann::json atts = nlohmann::json::array();
            for (const auto& a : attachments) {
                atts.push_back(nlohmann::json{{"id", a}});
            }
            j["attachments"] = atts;
        }
        
        return j;
    }
};

struct Message {
    std::string id;
    std::string channel;
    std::string author;                            // user ID
    std::optional<std::string> content;
    std::optional<std::string> edited_at;
    std::vector<std::string> mentions;
    std::vector<std::string> replies;
    nlohmann::json embeds;                         // raw
    nlohmann::json attachments;
    nlohmann::json raw;                            // original parsed object

    static Message from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Message Message::from_json(const nlohmann::json& j) {
    Message m;
    m.raw = j;
    if (j.contains("id") && j["id"].is_string()) m.id = j["id"].get<std::string>();
    
    if (j.contains("channel_id") && j["channel_id"].is_string()) m.channel = j["channel_id"].get<std::string>();
    else if (j.contains("channel") && j["channel"].is_string()) m.channel = j["channel"].get<std::string>();
    
    if (j.contains("author") && j["author"].is_object()) {
        if (j["author"].contains("id") && j["author"]["id"].is_string()) {
            m.author = j["author"]["id"].get<std::string>();
        }
    } else if (j.contains("author_id") && j["author_id"].is_string()) {
        m.author = j["author_id"].get<std::string>();
    } else if (j.contains("author") && j["author"].is_string()) {
        m.author = j["author"].get<std::string>();
    }
    
    if (j.contains("content") && j["content"].is_string()) m.content = j["content"].get<std::string>();
    if (j.contains("edited_timestamp") && j["edited_timestamp"].is_string()) m.edited_at = j["edited_timestamp"].get<std::string>();
    else if (j.contains("edited_at") && j["edited_at"].is_string()) m.edited_at = j["edited_at"].get<std::string>();
    
    if (j.contains("mentions") && j["mentions"].is_array()) {
        for (const auto& men : j["mentions"]) {
            if (men.is_string()) m.mentions.push_back(men.get<std::string>());
            else if (men.is_object() && men.contains("id") && men["id"].is_string()) m.mentions.push_back(men["id"].get<std::string>());
        }
    }
    
    if (j.contains("message_reference") && j["message_reference"].is_object()) {
        const auto& ref = j["message_reference"];
        if (ref.contains("message_id") && ref["message_id"].is_string()) {
            m.replies.push_back(ref["message_id"].get<std::string>());
        }
    } else if (j.contains("replies") && j["replies"].is_array()) {
        for (const auto& rep : j["replies"]) {
            if (rep.is_string()) m.replies.push_back(rep.get<std::string>());
        }
    }
    
    if (j.contains("embeds")) m.embeds = j["embeds"];
    else m.embeds = nlohmann::json::array();
    
    if (j.contains("attachments")) m.attachments = j["attachments"];
    else m.attachments = nlohmann::json::array();
    
    return m;
}

inline nlohmann::json Message::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["channel_id"] = channel;
    j["author_id"] = author;
    if (content) j["content"] = *content;
    if (edited_at) j["edited_timestamp"] = *edited_at;
    j["mentions"] = mentions;
    j["embeds"] = embeds;
    j["attachments"] = attachments;
    return j;
}

struct MessageQuery {
    std::optional<int> limit;
    std::optional<std::string> before;
    std::optional<std::string> after;
    std::optional<std::string> sort;
    std::optional<std::string> nearby;

    std::string to_query_string() const {
        std::string q = "";
        if (limit) q += (q.empty() ? "?" : "&") + std::string("limit=") + std::to_string(*limit);
        if (before) q += (q.empty() ? "?" : "&") + std::string("before=") + *before;
        if (after) q += (q.empty() ? "?" : "&") + std::string("after=") + *after;
        if (sort) q += (q.empty() ? "?" : "&") + std::string("sort=") + *sort;
        if (nearby) q += (q.empty() ? "?" : "&") + std::string("nearby=") + *nearby;
        return q;
    }
};

struct MessageSearchQuery {
    std::optional<std::string> query;
    std::optional<int> limit;
    std::optional<std::string> before;
    std::optional<std::string> after;
    std::optional<std::string> sort;
    std::optional<bool> pinned;
    std::optional<bool> include_users;

    nlohmann::json to_json() const {
        nlohmann::json j = nlohmann::json::object();
        if (query) j["query"] = *query;
        if (limit) j["limit"] = *limit;
        if (before) j["before"] = *before;
        if (after) j["after"] = *after;
        if (sort) j["sort"] = *sort;
        if (pinned) j["pinned"] = *pinned;
        if (include_users) j["include_users"] = *include_users;
        return j;
    }
};

} // namespace fluxerpp::models
