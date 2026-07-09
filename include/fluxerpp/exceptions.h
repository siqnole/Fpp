#pragma once
#include <stdexcept>
#include <string>

namespace fluxerpp {

class FluxerException : public std::runtime_error {
public:
    explicit FluxerException(const std::string& msg) : std::runtime_error(msg) {}
};

class AuthException : public FluxerException {
public:
    using FluxerException::FluxerException;
};

class RateLimitError : public FluxerException {
public:
    int retry_after_ms;
    RateLimitError(const std::string& msg, int retry_ms)
        : FluxerException(msg), retry_after_ms(retry_ms) {}
};

class NetworkError : public FluxerException {
public:
    using FluxerException::FluxerException;
};

class APIError : public FluxerException {
public:
    int http_status;
    APIError(const std::string& msg, int status)
        : FluxerException(msg), http_status(status) {}
};

} // namespace fluxerpp
