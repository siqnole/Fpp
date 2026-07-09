#pragma once
#include <string>
#include "../client_config.h"

namespace fluxerpp::utils {

class logger {
public:
    static void log(LogLevel level, const std::string& message, const ClientConfig& config);
};

} // namespace fluxerpp::utils
