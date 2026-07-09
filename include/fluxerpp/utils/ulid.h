#pragma once
#include <string>
#include <cstdint>

namespace fluxerpp::utils {

/**
 * Decodes the generation timestamp (millisecond epoch) from a Fluxer Snowflake.
 * Flushes Snowflake right-shifted by 22 bits + Fluxer Epoch (1420070400000).
 */
inline uint64_t decode_ulid_timestamp(const std::string& id_str) {
    if (id_str.empty()) return 0;
    try {
        uint64_t id = std::stoull(id_str);
        return (id >> 22) + 1420070400000ULL;
    } catch (...) {
        return 0;
    }
}

} // namespace fluxerpp::utils
