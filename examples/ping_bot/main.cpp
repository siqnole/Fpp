#include <fluxerpp/fluxerpp.h>
#include <iostream>

int main() {
    fluxerpp::ClientConfig cfg;
    cfg.log_level = fluxerpp::LogLevel::INFO;

    // Use command line or environment variable token if provided, otherwise default
    std::string token = "your_bot_token_here";
    const char* env_token = std::getenv("FLUXER_BOT_TOKEN");
    if (env_token) {
        token = env_token;
    }

    fluxerpp::cluster bot(token, cfg);

    bot.on_ready([](const fluxerpp::events::Ready& event) {
        std::cout << "[Ready] Logged in as " << event.user.username << "\n";
    });

    bot.on_message([&bot](const fluxerpp::events::Message& event) {
        // Ignore bots
        if (event.author.bot) return;

        if (event.content == "!ping") {
            bot.send_message(event.channel_id, "Pong! 🏓");
        }

        if (event.content == "!hello") {
            fluxerpp::models::MessagePayload payload;
            payload.content = "Hello, " + event.author.username + "!";
            payload.replies.push_back({event.id, true});
            bot.send_message(event.channel_id, payload);
        }
    });

    bot.on_error([](const fluxerpp::events::Error& e) {
        std::cerr << "[Gateway Error] " << e.error_id << "\n";
    });

    // Start bot
    if (token != "your_bot_token_here") {
        bot.start();  // blocks
    } else {
        std::cout << "Skipping bot connection start: please configure FLUXER_BOT_TOKEN environment variable.\n";
    }
    return 0;
}
