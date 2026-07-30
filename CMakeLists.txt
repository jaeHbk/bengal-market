cmake_minimum_required(VERSION 3.20)

set(BENGAL_MARKET_VERSION 0.1.0)

project(
  BengalMarket
  VERSION ${BENGAL_MARKET_VERSION}
  DESCRIPTION "Reproducible market-data capture and bounded-pipeline comparison"
  LANGUAGES CXX
)

include(CTest)
include(CheckCXXSourceRuns)
include(FetchContent)
include(GNUInstallDirs)

configure_file(
  include/bengal_market/version.hpp.in
  generated/bengal_market/version.hpp
  @ONLY
)

option(BENGAL_MARKET_BUILD_TESTS "Build bengal-market tests" ON)
option(BENGAL_MARKET_BUILD_LIVE "Build the Coinbase live capture adapter" ON)
option(
  BENGAL_MARKET_ENABLE_SANITIZERS
  "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
  OFF
)
set(
  BENGAL_SOURCE_DIR
  ""
  CACHE PATH
  "Use a local Bengal source tree instead of the pinned release"
)

set(BENGAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BENGAL_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BENGAL_BUILD_GOOGLE_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BENGAL_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(BENGAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if(BENGAL_SOURCE_DIR)
  add_subdirectory(
    "${BENGAL_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/_deps/bengal-local"
    EXCLUDE_FROM_ALL
  )
else()
  FetchContent_Declare(
    bengal
    GIT_REPOSITORY https://github.com/jaeHbk/bengal.git
    GIT_TAG 1bd5389db9fd802e4c836426dc94288f268cb543
    GIT_SHALLOW FALSE
  )
  FetchContent_MakeAvailable(bengal)
endif()

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
  GIT_SHALLOW FALSE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

find_package(Threads REQUIRED)

add_library(bengal_market_core STATIC src/core.cpp)
target_compile_features(bengal_market_core PUBLIC cxx_std_20)
target_include_directories(
  bengal_market_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(
  bengal_market_core
  PUBLIC Bengal::Core
  PRIVATE nlohmann_json::nlohmann_json Threads::Threads
)

if(BENGAL_MARKET_BUILD_LIVE)
  find_package(CURL 7.86 REQUIRED)
  set(CMAKE_REQUIRED_LIBRARIES CURL::libcurl)
  check_cxx_source_runs(
    [=[
      #include <curl/curl.h>
      #include <cstddef>

      int main() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
          return 1;
        }
        CURL* handle = curl_easy_init();
        if (handle == nullptr) {
          curl_global_cleanup();
          return 1;
        }
        std::size_t sent = 0;
        const auto result =
            curl_ws_send(handle, "", 0, &sent, 0, CURLWS_TEXT);
        curl_easy_cleanup(handle);
        curl_global_cleanup();
        return result == CURLE_NOT_BUILT_IN ? 1 : 0;
      }
    ]=]
    BENGAL_MARKET_CURL_HAS_WEBSOCKETS
  )
  unset(CMAKE_REQUIRED_LIBRARIES)
  if(NOT BENGAL_MARKET_CURL_HAS_WEBSOCKETS)
    message(FATAL_ERROR "libcurl must be built with WSS protocol support")
  endif()
  add_library(bengal_market_live STATIC src/live.cpp)
  target_compile_features(bengal_market_live PUBLIC cxx_std_20)
  target_include_directories(
    bengal_market_live
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(
    bengal_market_live
    PUBLIC bengal_market_core
    PRIVATE CURL::libcurl nlohmann_json::nlohmann_json
  )
  target_compile_definitions(
    bengal_market_live
    PUBLIC BENGAL_MARKET_HAS_LIVE=1
  )
endif()

add_executable(bengal-market src/main.cpp)
target_link_libraries(bengal-market PRIVATE bengal_market_core)
if(BENGAL_MARKET_BUILD_LIVE)
  target_link_libraries(bengal-market PRIVATE bengal_market_live)
endif()

set(bengal_market_targets bengal_market_core bengal-market)
if(TARGET bengal_market_live)
  list(APPEND bengal_market_targets bengal_market_live)
endif()

foreach(target IN LISTS bengal_market_targets)
  target_compile_options(
    ${target}
    PRIVATE
      $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Werror>
  )
endforeach()

if(BENGAL_MARKET_ENABLE_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "Sanitizers require GCC or Clang")
  endif()
  foreach(target IN LISTS bengal_market_targets)
    target_compile_options(
      ${target}
      PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()

if(BUILD_TESTING AND BENGAL_MARKET_BUILD_TESTS)
  add_executable(bengal_market_tests tests/tests.cpp)
  target_link_libraries(bengal_market_tests PRIVATE bengal_market_core)
  target_compile_options(
    bengal_market_tests
    PRIVATE
      $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Werror>
  )
  if(BENGAL_MARKET_ENABLE_SANITIZERS)
    target_compile_options(
      bengal_market_tests
      PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
    )
    target_link_options(
      bengal_market_tests
      PRIVATE -fsanitize=address,undefined
    )
  endif()
  add_test(NAME bengal_market_tests COMMAND bengal_market_tests)
endif()

install(TARGETS bengal-market RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE SECURITY.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs)

set(CPACK_PACKAGE_VENDOR "bengal-market contributors")
set(
  CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Read-only market-data capture and replay laboratory"
)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR TGZ)
include(CPack)
