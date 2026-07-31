cmake_minimum_required(VERSION 3.20)

set(BENGAL_MARKET_VERSION 0.2.2)

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

set(BENGAL_MARKET_SOURCE_REVISION "unknown")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE _bengal_market_git_result
    OUTPUT_VARIABLE _bengal_market_git_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(_bengal_market_git_result EQUAL 0)
    set(BENGAL_MARKET_SOURCE_REVISION "${_bengal_market_git_revision}")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" status --porcelain
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _bengal_market_git_status
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(_bengal_market_git_status)
      string(APPEND BENGAL_MARKET_SOURCE_REVISION "-dirty")
    endif()
  endif()
endif()
configure_file(
  include/bengal_market/build_info.hpp.in
  generated/bengal_market/build_info.hpp
  @ONLY
)

option(BENGAL_MARKET_BUILD_TESTS "Build bengal-market tests" ON)
option(BENGAL_MARKET_BUILD_LIVE "Build the Coinbase live capture adapter" ON)
option(
  BENGAL_MARKET_USE_SYSTEM_CURL
  "Use a system libcurl instead of the pinned WebSocket-enabled build"
  OFF
)
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

add_library(bengal_market_benchmark STATIC src/benchmark.cpp)
target_compile_features(bengal_market_benchmark PUBLIC cxx_std_20)
target_include_directories(
  bengal_market_benchmark
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(
  bengal_market_benchmark
  PUBLIC Bengal::Core
  PRIVATE nlohmann_json::nlohmann_json Threads::Threads
)

if(BENGAL_MARKET_BUILD_LIVE)
  if(BENGAL_MARKET_USE_SYSTEM_CURL)
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
      message(
        FATAL_ERROR
        "system libcurl was built without WebSocket support"
      )
    endif()
  else()
    set(_bengal_market_build_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_VERBOSE_STRINGS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_WEBSOCKETS OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_DICT ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_FILE ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_FTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_GOPHER ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_IMAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_IPFS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_MQTT ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_POP3 ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_RTSP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_SMB ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_SMTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TELNET ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TFTP ON CACHE BOOL "" FORCE)
    set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
    set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
    set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
    set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_GSSAPI OFF CACHE BOOL "" FORCE)
    set(USE_GSASL OFF CACHE BOOL "" FORCE)
    set(PICKY_COMPILER OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      curl
      GIT_REPOSITORY https://github.com/curl/curl.git
      GIT_TAG 400fffa90f30c7a2dc762fa33009d24851bd2016
      GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(curl)
    set(
      BUILD_TESTING
      "${_bengal_market_build_testing}"
      CACHE BOOL
      ""
      FORCE
    )
    unset(_bengal_market_build_testing)
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
target_link_libraries(
  bengal-market
  PRIVATE
    bengal_market_core
    bengal_market_benchmark
    nlohmann_json::nlohmann_json
)
if(BENGAL_MARKET_BUILD_LIVE)
  target_link_libraries(bengal-market PRIVATE bengal_market_live)
endif()

set(
  bengal_market_targets
  bengal_market_core
  bengal_market_benchmark
  bengal-market
)
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
  find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
  add_executable(bengal_market_tests tests/tests.cpp)
  target_link_libraries(bengal_market_tests PRIVATE bengal_market_core)
  target_include_directories(
    bengal_market_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
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
  add_test(
    NAME bengal_market_benchmark_cli
    COMMAND
      "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_cli_test.py"
      "$<TARGET_FILE:bengal-market>"
  )
  set_tests_properties(
    bengal_market_benchmark_cli
    PROPERTIES TIMEOUT 120
  )
  if(BENGAL_MARKET_BUILD_LIVE)
    add_test(
      NAME bengal_market_capture_signal
      COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/capture_signal_test.py"
        "$<TARGET_FILE:bengal-market>"
    )
    set_tests_properties(
      bengal_market_capture_signal
      PROPERTIES TIMEOUT 30
    )
  endif()
endif()

install(TARGETS bengal-market RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  FILES README.md LICENSE SECURITY.md THIRD_PARTY_NOTICES.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs)

set(CPACK_PACKAGE_VENDOR "bengal-market contributors")
set(
  CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Read-only market-data capture and replay laboratory"
)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR TGZ)
include(CPack)
