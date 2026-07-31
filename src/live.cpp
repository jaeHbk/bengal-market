#include <bengal_market/live.hpp>
#include <bengal_market/version.hpp>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "atomic_output.hpp"

namespace bengal_market {
namespace {

using json = nlohmann::json;
using clock_type = std::chrono::steady_clock;
constexpr std::size_t maximum_message_size = 4 * 1024 * 1024;
constexpr std::uint64_t maximum_consecutive_failures = 5;

class capture_stopped final : public std::exception {
 public:
  const char* what() const noexcept override {
    return "capture stop requested";
  }
};

bool stopped(const std::function<bool()>& stop_requested) noexcept {
  if (!stop_requested) {
    return false;
  }
  try {
    return stop_requested();
  } catch (...) {
    return true;
  }
}

struct curl_deleter {
  void operator()(CURL* handle) const noexcept {
    curl_easy_cleanup(handle);
  }
};

using curl_handle = std::unique_ptr<CURL, curl_deleter>;

class curl_global_state {
 public:
  curl_global_state() {
    const auto result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK) {
      throw std::runtime_error("curl global initialization failed");
    }
  }

  curl_global_state(const curl_global_state&) = delete;
  curl_global_state& operator=(const curl_global_state&) = delete;

  ~curl_global_state() {
    curl_global_cleanup();
  }
};

void check(CURLcode code, std::string_view operation) {
  if (code != CURLE_OK) {
    throw std::runtime_error(
        std::string(operation) + ": " + curl_easy_strerror(code));
  }
}

void wait_socket(CURL* handle,
                 short events,
                 int timeout_ms,
                 const std::function<bool()>& stop_requested) {
  if (stopped(stop_requested)) {
    throw capture_stopped{};
  }
  curl_socket_t socket = CURL_SOCKET_BAD;
  check(curl_easy_getinfo(handle, CURLINFO_ACTIVESOCKET, &socket),
        "get WebSocket socket");
  if (socket == CURL_SOCKET_BAD) {
    throw std::runtime_error("WebSocket has no active socket");
  }
  pollfd descriptor{socket, events, 0};
  const int result = poll(&descriptor, 1, timeout_ms);
  if (result < 0 && errno != EINTR) {
    throw std::runtime_error("poll failed");
  }
  if (stopped(stop_requested)) {
    throw capture_stopped{};
  }
}

void send_all(CURL* handle,
              std::string_view payload,
              unsigned int flags,
              const std::function<bool()>& stop_requested) {
  std::size_t offset = 0;
  while (offset < payload.size()) {
    if (stopped(stop_requested)) {
      throw capture_stopped{};
    }
    std::size_t sent = 0;
    const auto result =
        curl_ws_send(handle,
                     payload.data() + offset,
                     payload.size() - offset,
                     &sent,
                     0,
                     flags);
    if (result == CURLE_AGAIN) {
      offset += sent;
      if (offset < payload.size()) {
        wait_socket(handle, POLLOUT, 250, stop_requested);
      }
      continue;
    }
    check(result, "send WebSocket frame");
    if (sent == 0) {
      throw std::runtime_error("WebSocket send made no progress");
    }
    offset += sent;
  }
}

int progress_callback(void* context,
                      curl_off_t,
                      curl_off_t,
                      curl_off_t,
                      curl_off_t) noexcept {
  const auto* stop_requested =
      static_cast<const std::function<bool()>*>(context);
  return stopped(*stop_requested) ? 1 : 0;
}

curl_handle connect(
    const capture_options& options,
    const std::function<bool()>& stop_requested) {
  curl_handle handle(curl_easy_init());
  if (!handle) {
    throw std::runtime_error("curl handle allocation failed");
  }
  check(curl_easy_setopt(
            handle.get(), CURLOPT_URL, options.endpoint.c_str()),
        "set endpoint");
  check(curl_easy_setopt(handle.get(), CURLOPT_CONNECT_ONLY, 2L),
        "enable WebSocket mode");
  check(curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT, 10L),
        "set connect timeout");
  check(curl_easy_setopt(handle.get(), CURLOPT_NOPROGRESS, 0L),
        "enable stop callback");
  check(curl_easy_setopt(
            handle.get(), CURLOPT_XFERINFOFUNCTION, progress_callback),
        "set stop callback");
  check(curl_easy_setopt(
            handle.get(), CURLOPT_XFERINFODATA, &stop_requested),
        "set stop callback state");
  const auto user_agent = "bengal-market/" + std::string(version);
  check(curl_easy_setopt(
            handle.get(), CURLOPT_USERAGENT, user_agent.c_str()),
        "set user agent");
  const auto connected = curl_easy_perform(handle.get());
  if (connected == CURLE_ABORTED_BY_CALLBACK &&
      stopped(stop_requested)) {
    throw capture_stopped{};
  }
  check(connected, "connect WebSocket");

  const auto subscribe =
      json{{"type", "subscribe"},
           {"product_ids", options.product_ids},
          {"channel", "market_trades"}}
          .dump();
  send_all(handle.get(), subscribe, CURLWS_TEXT, stop_requested);
  const auto heartbeat =
      json{{"type", "subscribe"},
           {"product_ids", options.product_ids},
          {"channel", "heartbeats"}}
          .dump();
  send_all(handle.get(), heartbeat, CURLWS_TEXT, stop_requested);
  return handle;
}

std::uint64_t wall_time_ns() noexcept {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

}  // namespace

capture_result capture_live(
    const capture_options& options,
    const std::function<bool()>& stop_requested) {
  if (options.output.empty() || options.product_ids.empty() ||
      options.duration.count() <= 0) {
    throw std::invalid_argument(
        "capture output, products, and duration are required");
  }
  for (const auto& product : options.product_ids) {
    if (product.empty() || product.size() > 20) {
      throw std::invalid_argument(
          "product IDs must contain 1 to 20 characters");
    }
  }

  curl_global_state curl_state;
  detail::atomic_output output(options.output);
  output.write_line(
      json{{"type", "metadata"},
           {"format", "bengal-market-capture"},
           {"version", 1},
           {"source", "coinbase-advanced-trade"},
           {"endpoint", options.endpoint},
           {"product_ids", options.product_ids}}
          .dump());

  capture_result result;
  std::string last_error;
  std::uint64_t connection_id = 0;
  std::uint64_t consecutive_failures = 0;
  const auto deadline = clock_type::now() + options.duration;
  while (clock_type::now() < deadline && !stopped(stop_requested)) {
    ++connection_id;
    try {
      auto handle = connect(options, stop_requested);
      std::string message;
      std::array<char, 64 * 1024> buffer{};
      while (clock_type::now() < deadline &&
             !stopped(stop_requested)) {
        std::size_t received = 0;
        const curl_ws_frame* metadata = nullptr;
        const auto code =
            curl_ws_recv(handle.get(),
                         buffer.data(),
                         buffer.size(),
                         &received,
                         &metadata);
        if (code == CURLE_AGAIN) {
          wait_socket(handle.get(), POLLIN, 250, stop_requested);
          continue;
        }
        check(code, "receive WebSocket frame");
        if (metadata == nullptr) {
          throw std::runtime_error(
              "WebSocket frame metadata missing");
        }
        if ((metadata->flags & CURLWS_CLOSE) != 0U) {
          throw std::runtime_error("server closed WebSocket");
        }
        if ((metadata->flags & (CURLWS_PING | CURLWS_PONG)) != 0U) {
          continue;
        }
        if ((metadata->flags & CURLWS_BINARY) != 0U) {
          message.clear();
          continue;
        }
        if ((metadata->flags & CURLWS_TEXT) == 0U) {
          continue;
        }
        if (received > maximum_message_size - message.size()) {
          throw std::runtime_error(
              "WebSocket message exceeds 4 MiB limit");
        }
        message.append(buffer.data(), received);
        if (metadata->bytesleft != 0) {
          continue;
        }
        if ((metadata->flags & CURLWS_CONT) != 0U) {
          continue;
        }
        output.write_line(
            json{{"type", "frame"},
                 {"received_ns", wall_time_ns()},
                 {"connection_id", connection_id},
                 {"payload", message}}
                .dump());
        ++result.frames;
        consecutive_failures = 0;
        message.clear();
      }
    } catch (const capture_stopped&) {
      break;
    } catch (const std::exception& error) {
      last_error = error.what();
      if (clock_type::now() >= deadline ||
          stopped(stop_requested)) {
        break;
      }
      ++result.reconnects;
      ++consecutive_failures;
      if (consecutive_failures >= maximum_consecutive_failures) {
        throw std::runtime_error(
            "capture stopped after repeated failures: " + last_error);
      }
      for (int attempt = 0;
           attempt < 25 && !stopped(stop_requested);
           ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }
  result.interrupted = stopped(stop_requested);
  if (result.frames == 0 && !result.interrupted) {
    throw std::runtime_error(
        "capture received no frames" +
        (last_error.empty() ? std::string{}
                            : std::string(": ") + last_error));
  }
  output.commit();
  return result;
}

}  // namespace bengal_market
