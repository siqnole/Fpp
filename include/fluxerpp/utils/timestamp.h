#pragma once
#include <string>

namespace fluxerpp::utils {

/**
 * Formats a Unix timestamp into a chat/Discord-compatible tag (e.g. <t:1782830580:R>)
 */
inline std::string format_timestamp(int64_t unix_time, const std::string& style = "R") {
    return "<t:" + std::to_string(unix_time) + ":" + style + ">";
}

} // namespace fluxerpp::utils
