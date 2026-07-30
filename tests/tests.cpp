#include <bengal_market/model.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

void decimal_tests() {
  check(bengal_market::parse_decimal("0") == 0, "zero decimal");
  check(bengal_market::parse_decimal("1.25") == 125'000'000,
        "fractional decimal");
  check(bengal_market::parse_decimal("-0.00000001") == -1,
        "negative minimum quantum");
  check(!bengal_market::parse_decimal("1.000000001"),
        "reject excess precision");
  check(!bengal_market::parse_decimal("1x"),
        "reject non-decimal input");
  check(!bengal_market::parse_decimal(""), "reject empty input");
  check(bengal_market::format_decimal(125'000'000) == "1.25",
        "canonical decimal formatting");
  check(
      bengal_market::format_decimal(
          std::numeric_limits<std::int64_t>::min()) ==
          "-92233720368.54775808",
      "minimum fixed-point formatting");
}

void replay_tests() {
  const auto unique =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path =
      std::filesystem::temp_directory_path() /
      ("bengal-market-test-" + std::to_string(unique) + ".jsonl");
  bengal_market::generate_fixture(path, 10'000, "BTC-USD");
  const auto bengal = bengal_market::replay(
      path, bengal_market::pipeline_engine::bengal);
  const auto standard = bengal_market::replay(
      path, bengal_market::pipeline_engine::standard);
  check(bengal.events == 10'000,
        "Bengal processes every generated event");
  check(standard.events == bengal.events,
        "engines process equal event counts");
  check(standard.checksum == bengal.checksum,
        "engines produce equal checksums");
  check(bengal.input.parse_errors == 0,
        "generated fixture parses cleanly");
  check(bengal.input.sequence_gaps == 0,
        "generated fixture has no gaps");
  check(bengal.stage_latency.samples == bengal.events,
        "latency sample exists for every event");
  std::filesystem::remove(path);
}

void sequence_tests() {
  const auto unique =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path =
      std::filesystem::temp_directory_path() /
      ("bengal-market-sequence-test-" + std::to_string(unique) +
       ".jsonl");
  std::ofstream stream(path);
  stream
      << R"({"type":"metadata","format":"bengal-market-capture","version":1})"
      << '\n'
      << R"({"type":"frame","received_ns":1,"connection_id":1,"payload":"{\"channel\":\"heartbeats\",\"sequence_num\":40}"})"
      << '\n'
      << R"({"type":"frame","received_ns":2,"connection_id":1,"payload":"{\"channel\":\"market_trades\",\"sequence_num\":7,\"events\":[]}"})"
      << '\n'
      << R"({"type":"frame","received_ns":3,"connection_id":1,"payload":"{\"channel\":\"market_trades\",\"sequence_num\":9,\"events\":[]}"})"
      << '\n';
  stream.close();
  const auto report = bengal_market::replay(
      path, bengal_market::pipeline_engine::bengal);
  check(report.input.sequence_gaps == 1,
        "sequence gaps are tracked independently per channel");
  check(report.input.parse_errors == 0,
        "heartbeat frames do not cause parse errors");
  std::filesystem::remove(path);
}

}  // namespace

int main() {
  try {
    decimal_tests();
    replay_tests();
    sequence_tests();
  } catch (const std::exception& error) {
    std::cerr << "unexpected exception: " << error.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
