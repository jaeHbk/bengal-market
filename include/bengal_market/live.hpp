#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace bengal_market {

struct capture_options {
  std::filesystem::path output;
  std::vector<std::string> product_ids;
  std::chrono::seconds duration{30};
  std::string endpoint{"wss://advanced-trade-ws.coinbase.com"};
};

struct capture_result {
  std::uint64_t frames{0};
  std::uint64_t reconnects{0};
};

#if defined(BENGAL_MARKET_HAS_LIVE)
capture_result capture_live(const capture_options& options);
#endif

}  // namespace bengal_market
