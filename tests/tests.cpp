#include <bengal_market/model.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
      << R"({"type":"metadata","format":"bengal-market-capture","version":1,"source":"test","product_ids":["BTC-USD"]})"
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

std::filesystem::path write_trade_capture(
    std::string_view name,
    std::string_view timestamp,
    bool include_sequence = true,
    bool include_receipt = true) {
  const auto unique =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path =
      std::filesystem::temp_directory_path() /
      (std::string(name) + "-" + std::to_string(unique) + ".jsonl");
  const std::string payload =
      "{\"channel\":\"market_trades\"" +
      std::string(include_sequence ? ",\"sequence_num\":1" : "") +
      ",\"events\":[{\"type\":\"update\",\"trades\":[{"
      "\"trade_id\":\"1\",\"product_id\":\"BTC-USD\","
      "\"price\":\"30000\",\"size\":\"0.1\",\"side\":\"BUY\","
      "\"time\":\"" +
      std::string(timestamp) + "\"}]}]}";
  std::ofstream stream(path);
  stream
      << R"({"type":"metadata","format":"bengal-market-capture","version":1,"source":"test","product_ids":["BTC-USD"]})"
      << '\n'
      << "{\"type\":\"frame\""
      << (include_receipt ? ",\"received_ns\":1" : "")
      << ",\"connection_id\":1,\"payload\":" << std::quoted(payload)
      << "}\n";
  return path;
}

void validity_tests() {
  const auto first =
      write_trade_capture("bengal-market-time-a",
                          "2026-01-01T00:00:00.000000001Z");
  const auto second =
      write_trade_capture("bengal-market-time-b",
                          "2027-01-01T00:00:00.000000001Z");
  const auto first_report = bengal_market::replay(
      first, bengal_market::pipeline_engine::bengal);
  const auto second_report = bengal_market::replay(
      second, bengal_market::pipeline_engine::bengal);
  check(first_report.input.parse_errors == 0,
        "strict RFC 3339 timestamp parses");
  check(first_report.checksum != second_report.checksum,
        "full source timestamp contributes to checksum");

  const auto missing_sequence =
      write_trade_capture("bengal-market-missing-sequence",
                          "2026-01-01T00:00:00Z",
                          false);
  const auto malformed = bengal_market::replay(
      missing_sequence, bengal_market::pipeline_engine::bengal);
  check(malformed.input.parse_errors == 1,
        "market frames require a sequence number");
  check(!bengal_market::reports_comparable(malformed, malformed),
        "parse errors invalidate comparison");

  const auto missing_receipt =
      write_trade_capture("bengal-market-missing-receipt",
                          "2026-01-01T00:00:00Z",
                          true,
                          false);
  const auto invalid_envelope = bengal_market::replay(
      missing_receipt, bengal_market::pipeline_engine::bengal);
  check(invalid_envelope.input.parse_errors == 1,
        "frame envelope requires receipt time");

  const auto invalid_date =
      write_trade_capture("bengal-market-invalid-date",
                          "2026-02-30T00:00:00Z");
  const auto invalid_timestamp = bengal_market::replay(
      invalid_date, bengal_market::pipeline_engine::bengal);
  check(invalid_timestamp.input.parse_errors == 1,
        "invalid calendar timestamps are rejected");

  std::filesystem::remove(first);
  std::filesystem::remove(second);
  std::filesystem::remove(missing_sequence);
  std::filesystem::remove(missing_receipt);
  std::filesystem::remove(invalid_date);
}

}  // namespace

int main() {
  try {
    decimal_tests();
    replay_tests();
    sequence_tests();
    validity_tests();
  } catch (const std::exception& error) {
    std::cerr << "unexpected exception: " << error.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
