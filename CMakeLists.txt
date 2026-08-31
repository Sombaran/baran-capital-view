cmake_minimum_required(VERSION 3.16)

project(portfolio_health VERSION 2.0.12 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(PORTFOLIO_HEALTH_ENABLE_IPO "Enable interprocedural optimization" OFF)
option(PORTFOLIO_HEALTH_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Dependencies ---------------------------------------------------------
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL QUIET)
find_package(uuid QUIET)  # Optional for NotificationService (v2.0.0+)

# nlohmann/json is managed through Conan dependency management (v2.0.10+)
# See conanfile.py for version specification
find_package(nlohmann_json REQUIRED)

# --- Shared library + application targets ---------------------------------
set(PORTFOLIO_HEALTH_CORE_SOURCES
  src/HttpClient.cpp
  src/UpstoxClient.cpp
  src/StreamingClient.cpp
  src/WebServer.cpp
  src/PortfolioHealth.cpp
  src/PortfolioLoader.cpp
  src/TechnicalIndicators.cpp
  src/MLModels.cpp
  src/SignalGenerator.cpp
  src/OrdersAPI.cpp
  src/NotificationService.cpp
  src/SecureUtils.cpp
  src/TaskScheduler.cpp
)

add_library(portfolio_health_core SHARED
  ${PORTFOLIO_HEALTH_CORE_SOURCES}
)

target_include_directories(portfolio_health_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(portfolio_health_core
  PRIVATE PORTFOLIO_HEALTH_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(portfolio_health_core
  PUBLIC
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# Link OpenSSL if available
if(OpenSSL_FOUND)
  target_link_libraries(portfolio_health_core PUBLIC OpenSSL::Crypto OpenSSL::SSL)
  target_compile_definitions(portfolio_health_core PRIVATE HAVE_OPENSSL=1)
else()
  message(STATUS "OpenSSL development files not found; using libcurl TLS and portable SecureUtils fallbacks")
endif()

# Link uuid if available
if(uuid_FOUND)
  target_link_libraries(portfolio_health_core PUBLIC uuid)
  target_compile_definitions(portfolio_health_core PRIVATE HAVE_UUID_H=1)
endif()

add_executable(portfolio_health
  src/main.cpp
)

target_link_libraries(portfolio_health
  PRIVATE portfolio_health_core
)

target_compile_definitions(portfolio_health
  PRIVATE PORTFOLIO_HEALTH_VERSION="${PROJECT_VERSION}"
)

if(PORTFOLIO_HEALTH_ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set_property(TARGET portfolio_health_core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET portfolio_health PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(FATAL_ERROR "IPO is not supported: ${ipo_error}")
  endif()
endif()

if(PORTFOLIO_HEALTH_ENABLE_SANITIZERS AND NOT MSVC)
  target_compile_options(portfolio_health_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(portfolio_health_core PRIVATE -fsanitize=address,undefined)
  target_compile_options(portfolio_health PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(portfolio_health PRIVATE -fsanitize=address,undefined)
endif()

if(MSVC)
  target_compile_options(portfolio_health_core PRIVATE /W4 /utf-8 /permissive-)
  target_compile_options(portfolio_health PRIVATE /W4 /utf-8 /permissive-)
  target_compile_definitions(portfolio_health_core PRIVATE
    _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_definitions(portfolio_health PRIVATE
    _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  target_compile_options(portfolio_health_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(portfolio_health PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  if(NOT TARGET GTest::gtest_main)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(portfolio_health_tests
    tests/cpp/test_security.cpp
  )

  target_include_directories(portfolio_health_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(portfolio_health_tests
    PRIVATE portfolio_health_core GTest::gtest_main
  )

  add_test(NAME portfolio_health_tests COMMAND portfolio_health_tests)
endif()

# --- Convenience run targets ---------------------------------------------
# Usage (after configuring the build tree once):
#   cmake --build build --target run          # sample offline data
#   cmake --build build --target run-csv      # your config/my_portfolio.csv
#   cmake --build build --target run-json     # your config/my_portfolio.json
#   cmake --build build --target run-live     # live account (needs token)
add_custom_target(run
  COMMAND portfolio_health --file ${CMAKE_CURRENT_SOURCE_DIR}/config/sample_positions.json
  DEPENDS portfolio_health
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  USES_TERMINAL
  COMMENT "Running portfolio_health with sample offline data")

add_custom_target(run-csv
  COMMAND portfolio_health --portfolio ${CMAKE_CURRENT_SOURCE_DIR}/config/my_portfolio.csv
  DEPENDS portfolio_health
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  USES_TERMINAL
  COMMENT "Running portfolio_health with config/my_portfolio.csv")

add_custom_target(run-json
  COMMAND portfolio_health --portfolio ${CMAKE_CURRENT_SOURCE_DIR}/config/my_portfolio.json
  DEPENDS portfolio_health
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  USES_TERMINAL
  COMMENT "Running portfolio_health with config/my_portfolio.json")

add_custom_target(run-live
  COMMAND portfolio_health
  DEPENDS portfolio_health
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  USES_TERMINAL
  COMMENT "Running portfolio_health against the live Upstox account (needs UPSTOX_ACCESS_TOKEN)")
