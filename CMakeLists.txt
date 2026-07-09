cmake_minimum_required(VERSION 3.16)
project(fluxerpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(JSON_DIAGNOSTICS=1)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

option(FLUXERPP_BUILD_EXAMPLES "Build example bots" ON)
option(FLUXERPP_USE_BOOST_BEAST "Use Boost.Beast instead of ixwebsocket" OFF)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# cpp-httplib (header-only)
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.0
)
FetchContent_MakeAvailable(httplib)

if(NOT FLUXERPP_USE_BOOST_BEAST)
    # ixwebsocket
    set(USE_TLS ON CACHE BOOL "Enable TLS/SSL support" FORCE)
    FetchContent_Declare(ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG        v11.4.5
    )
    FetchContent_MakeAvailable(ixwebsocket)
endif()

find_package(OpenSSL REQUIRED)

# fluxerpp library target
add_library(fluxerpp
    src/cluster.cpp
    src/rest.cpp
    src/gateway.cpp
    src/event_handler.cpp
    src/utils/ratelimiter.cpp
    src/utils/logger.cpp
)

target_include_directories(fluxerpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(fluxerpp PUBLIC
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(NOT FLUXERPP_USE_BOOST_BEAST)
    target_link_libraries(fluxerpp PUBLIC ixwebsocket)
    target_compile_definitions(fluxerpp PUBLIC FLUXERPP_USE_IXWEBSOCKET)
else()
    find_package(Boost REQUIRED COMPONENTS system)
    target_link_libraries(fluxerpp PUBLIC Boost::system)
    target_compile_definitions(fluxerpp PUBLIC FLUXERPP_USE_BOOST_BEAST)
endif()

if(FLUXERPP_BUILD_EXAMPLES)
    add_subdirectory(examples/ping_bot)
endif()
