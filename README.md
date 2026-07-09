# fluxerpp

a simple c++17 library for the [Fluxer API](https://fluxer.app).

## requirements

- c++17 compiler
- cmake (3.16+)
- git
- openssl

## installation

add it as a subdirectory in your cmake project:

then add to your `CMakeLists.txt`:
```cmake
add_subdirectory(fluxer)
target_link_libraries(your_target PRIVATE fluxerpp)
```

---

## setup with linux (debian/ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake git libssl-dev

mkdir build && cd build
cmake ..
cmake --build .
```

---

## simple example

```cpp
#include <fluxerpp/fluxerpp.h>
#include <iostream>

int main() {
    fluxerpp::ClientConfig cfg;
    cfg.log_level = fluxerpp::LogLevel::INFO;

    fluxerpp::cluster bot("YOUR_BOT_TOKEN_HERE", cfg);

    bot.on_ready([](const fluxerpp::events::Ready& event) {
        std::cout << "logged in as " << event.user.username << "\n";
    });

    bot.on_message([&bot](const fluxerpp::events::Message& event) {
        if (event.author.bot) return;

        if (event.content == "!ping") {
            bot.send_message(event.channel_id, "pong!");
        }
    });

    bot.start();
    return 0;
}
```
