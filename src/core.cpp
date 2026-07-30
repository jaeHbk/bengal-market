#include <bengal_market/model.hpp>

#include <bengal/concurrency/spsc_queue.hpp>
#include <bengal/version.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace bengal_market {
namespace {

using json = nlohmann::json;
using clock_type = std::chrono::steady_clock;
constexpr std::size_t maximum_record_size = 8 * 1024 * 1024;
constexpr std::size_t maximum_sequence_streams = 4096;

std::uint64_t now_ns() noexcept {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          clock_type::now().time_since_epoch())
          .count());
}

class latency_histogram {
 public:
  void observe(std::uint64_t value) noexcept {
    const auto bucket =
        value == 0
            ? std::size_t{0}
            : std::min<std::size_t>(
                  static_cast<std::size_t>(
                      std::bit_width(value - 1) + 1),
                  buckets_.size() - 1);
    ++buckets_[bucket];
    ++samples_;
    minimum_ = std::min(minimum_, value);
    maximum_ = std::max(maximum_, value);
  }

  latency_summary summary() const noexcept {
    if (samples_ == 0) {
      return {};
    }
    return {samples_,
            minimum_,
            quantile(50),
            quantile(95),
            quantile(99),
            maximum_};
  }

 private:
  std::uint64_t quantile(std::uint64_t percentile) const noexcept {
    const auto target = (samples_ * percentile + 99) / 100;
    std::uint64_t cumulative = 0;
    for (std::size_t index = 0; index < buckets_.size(); ++index) {
      cumulative += buckets_[index];
      if (cumulative >= target) {
        if (index == 0) {
          return 0;
        }
        if (index > 64) {
          return std::numeric_limits<std::uint64_t>::max();
        }
        return std::uint64_t{1} << (index - 1);
      }
    }
    return maximum_;
  }

  std::array<std::uint64_t, 65> buckets_{};
  std::uint64_t samples_{0};
  std::uint64_t minimum_{std::numeric_limits<std::uint64_t>::max()};
  std::uint64_t maximum_{0};
};

void hash_bytes(std::uint64_t& hash, std::string_view value) noexcept {
  for (const unsigned char byte : value) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
  using unsigned_type = std::make_unsigned_t<Integer>;
  auto bits = static_cast<unsigned_type>(value);
  for (std::size_t index = 0; index < sizeof(bits); ++index) {
    hash ^= static_cast<unsigned char>(bits & 0xffU);
    hash *= 1099511628211ULL;
    if constexpr (sizeof(unsigned_type) > 1) {
      bits >>= 8U;
    }
  }
}

std::uint64_t update_checksum(std::uint64_t hash,
                              const trade& value) noexcept {
  hash_bytes(hash, value.trade_id.view());
  hash_bytes(hash, value.product_id.view());
  hash_integer(hash, value.price);
  hash_integer(hash, value.size);
  hash_integer(hash, static_cast<std::uint8_t>(value.side));
  hash_integer(hash, value.source_time_ns);
  return hash;
}

template <typename T, std::size_t Capacity>
class standard_spsc_queue {
 public:
  bool try_push(T&& value) {
    std::lock_guard lock(mutex_);
    if (size_ == Capacity) {
      return false;
    }
    slots_[write_].emplace(std::move(value));
    write_ = (write_ + 1) % Capacity;
    ++size_;
    return true;
  }

  std::optional<T> try_pop() {
    std::lock_guard lock(mutex_);
    if (size_ == 0) {
      return std::nullopt;
    }
    std::optional<T> value(std::move(slots_[read_]));
    slots_[read_].reset();
    read_ = (read_ + 1) % Capacity;
    --size_;
    return value;
  }

 private:
  std::array<std::optional<T>, Capacity> slots_{};
  std::mutex mutex_;
  std::size_t read_{0};
  std::size_t write_{0};
  std::size_t size_{0};
};

template <typename Queue>
class pipeline {
 public:
  pipeline() : worker_([this] { consume(); }) {}

  pipeline(const pipeline&) = delete;
  pipeline& operator=(const pipeline&) = delete;

  ~pipeline() {
    finish();
  }

  void submit(trade value) {
    value.enqueued_ns = now_ns();
    while (!queue_.try_push(std::move(value))) {
      ++backpressure_;
      std::this_thread::yield();
    }
  }

  void finish() {
    if (!finished_) {
      producer_done_.store(true, std::memory_order_release);
      worker_.join();
      finished_ = true;
    }
  }

  replay_report report(std::string engine_name,
                       parse_metrics input,
                       std::uint64_t elapsed_ns) const {
    replay_report value;
    value.engine = std::move(engine_name);
    value.input = input;
    value.stage_latency = histogram_.summary();
    value.events = events_;
    value.dropped = 0;
    value.backpressure = backpressure_;
    value.checksum = checksum_;
    value.elapsed_ns = elapsed_ns;
    return value;
  }

 private:
  void consume() noexcept {
    for (;;) {
      auto value = queue_.try_pop();
      if (value) {
        histogram_.observe(now_ns() - value->enqueued_ns);
        checksum_ = update_checksum(checksum_, *value);
        ++events_;
        continue;
      }
      if (producer_done_.load(std::memory_order_acquire)) {
        break;
      }
      std::this_thread::yield();
    }
  }

  Queue queue_;
  std::atomic<bool> producer_done_{false};
  std::thread worker_;
  latency_histogram histogram_;
  std::uint64_t events_{0};
  std::uint64_t backpressure_{0};
  std::uint64_t checksum_{14695981039346656037ULL};
  bool finished_{false};
};

std::optional<unsigned> parse_component(std::string_view value,
                                        std::size_t offset,
                                        std::size_t length) noexcept {
  if (offset + length > value.size()) {
    return std::nullopt;
  }
  unsigned result = 0;
  for (std::size_t index = offset; index < offset + length; ++index) {
    const char character = value[index];
    if (character < '0' || character > '9') {
      return std::nullopt;
    }
    result = result * 10 + static_cast<unsigned>(character - '0');
  }
  return result;
}

bool is_leap_year(unsigned year) noexcept {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

unsigned days_in_month(unsigned year, unsigned month) noexcept {
  constexpr std::array<unsigned, 12> days{
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return days[month - 1];
}

std::int64_t days_from_civil(int year,
                             unsigned month,
                             unsigned day) noexcept {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const auto year_of_era =
      static_cast<unsigned>(year - era * 400);
  const auto day_of_year =
      (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 +
      day - 1;
  const auto day_of_era =
      year_of_era * 365 + year_of_era / 4 -
      year_of_era / 100 + day_of_year;
  return static_cast<std::int64_t>(era) * 146097 +
         static_cast<std::int64_t>(day_of_era) - 719468;
}

std::optional<std::uint64_t> parse_timestamp_ns(
    std::string_view value) noexcept {
  if (value.size() < 20 || value[4] != '-' || value[7] != '-' ||
      value[10] != 'T' || value[13] != ':' || value[16] != ':') {
    return std::nullopt;
  }
  const auto year = parse_component(value, 0, 4);
  const auto month = parse_component(value, 5, 2);
  const auto day = parse_component(value, 8, 2);
  const auto hour = parse_component(value, 11, 2);
  const auto minute = parse_component(value, 14, 2);
  const auto second = parse_component(value, 17, 2);
  if (!year || !month || !day || !hour || !minute || !second ||
      *year < 1970 || *month < 1 || *month > 12 ||
      *day < 1 || *day > days_in_month(*year, *month) ||
      *hour > 23 || *minute > 59 || *second > 59) {
    return std::nullopt;
  }

  std::uint64_t fraction = 0;
  std::size_t fraction_digits = 0;
  std::size_t cursor = 19;
  if (value[cursor] == '.') {
    ++cursor;
    while (cursor < value.size() && value[cursor] != 'Z') {
      if (fraction_digits == 9 ||
          value[cursor] < '0' || value[cursor] > '9') {
        return std::nullopt;
      }
      fraction =
          fraction * 10 + static_cast<unsigned>(value[cursor] - '0');
      ++fraction_digits;
      ++cursor;
    }
    if (fraction_digits == 0) {
      return std::nullopt;
    }
  }
  if (cursor + 1 != value.size() || value[cursor] != 'Z') {
    return std::nullopt;
  }
  while (fraction_digits < 9) {
    fraction *= 10;
    ++fraction_digits;
  }

  const auto days =
      days_from_civil(static_cast<int>(*year), *month, *day);
  if (days < 0) {
    return std::nullopt;
  }
  const auto seconds =
      static_cast<std::uint64_t>(days) * 86'400 +
      static_cast<std::uint64_t>(*hour) * 3'600 +
      static_cast<std::uint64_t>(*minute) * 60 + *second;
  if (seconds >
      (std::numeric_limits<std::uint64_t>::max() - fraction) /
          1'000'000'000ULL) {
    return std::nullopt;
  }
  return seconds * 1'000'000'000ULL + fraction;
}

bool read_bounded_line(std::istream& stream, std::string& line) {
  line.clear();
  char character = '\0';
  while (stream.get(character)) {
    if (character == '\n') {
      return true;
    }
    if (line.size() == maximum_record_size) {
      throw std::runtime_error("capture record exceeds 8 MiB limit");
    }
    line.push_back(character);
  }
  return !line.empty();
}

template <typename Consumer>
parse_metrics read_capture(const std::filesystem::path& input,
                           Consumer&& consumer) {
  std::ifstream stream(input);
  if (!stream) {
    throw std::runtime_error("cannot open capture: " + input.string());
  }

  parse_metrics metrics;
  std::unordered_map<std::uint64_t, std::uint64_t> last_sequences;
  bool metadata_seen = false;
  std::string line;
  line.reserve(64 * 1024);
  while (read_bounded_line(stream, line)) {
    if (line.empty()) {
      continue;
    }
    try {
      const auto record = json::parse(line);
      const auto type = record.at("type").get<std::string>();
      if (type == "metadata") {
        if (metadata_seen ||
            record.at("format") != "bengal-market-capture" ||
            record.at("version") != 1) {
          throw std::runtime_error("unsupported capture metadata");
        }
        (void)record.at("source").get_ref<const std::string&>();
        const auto& product_ids = record.at("product_ids");
        if (!product_ids.is_array() || product_ids.empty()) {
          throw std::runtime_error(
              "capture metadata requires product IDs");
        }
        for (const auto& product_id : product_ids) {
          const auto& product =
              product_id.get_ref<const std::string&>();
          if (product.empty() || product.size() > 20) {
            throw std::runtime_error(
                "invalid metadata product ID");
          }
        }
        metadata_seen = true;
        continue;
      }
      if (type != "frame") {
        throw std::runtime_error("unknown capture record type");
      }
      if (!metadata_seen) {
        throw std::runtime_error("capture metadata must be first");
      }

      ++metrics.frames;
      (void)record.at("received_ns").get<std::uint64_t>();
      const auto connection_id =
          record.at("connection_id").get<std::uint64_t>();
      const auto payload =
          json::parse(record.at("payload").get<std::string>());
      const auto channel = payload.at("channel").get<std::string>();
      if (!payload.contains("sequence_num")) {
        if (channel == "market_trades" || channel == "heartbeats") {
          throw std::runtime_error(
              "data channel payload has no sequence number");
        }
        continue;
      }
      const auto sequence =
          payload.at("sequence_num").get<std::uint64_t>();
      const auto previous = last_sequences.find(connection_id);
      if (previous == last_sequences.end()) {
        if (last_sequences.size() == maximum_sequence_streams) {
          throw std::runtime_error(
              "capture exceeds sequence-stream limit");
        }
        last_sequences.emplace(connection_id, sequence);
      } else {
        if (sequence > previous->second &&
            sequence - previous->second > 1) {
          metrics.sequence_gaps += sequence - previous->second - 1;
        } else if (sequence <= previous->second) {
          ++metrics.out_of_order;
        }
        if (sequence > previous->second) {
          previous->second = sequence;
        }
      }

      if (channel != "market_trades") {
        continue;
      }
      for (const auto& event : payload.at("events")) {
        for (const auto& input_trade : event.at("trades")) {
          trade value;
          const auto price = parse_decimal(
              input_trade.at("price").get_ref<const std::string&>());
          const auto size = parse_decimal(
              input_trade.at("size").get_ref<const std::string&>());
          const auto side = input_trade.at("side").get<std::string>();
          const auto timestamp = parse_timestamp_ns(
              input_trade.at("time").get_ref<const std::string&>());
          if (!price || !size || !timestamp ||
              !value.trade_id.assign(
                  input_trade.at("trade_id")
                      .get_ref<const std::string&>()) ||
              !value.product_id.assign(
                  input_trade.at("product_id")
                      .get_ref<const std::string&>()) ||
              (side != "BUY" && side != "SELL")) {
            throw std::runtime_error("invalid market trade");
          }
          value.price = *price;
          value.size = *size;
          value.side =
              side == "BUY" ? trade_side::buy : trade_side::sell;
          value.source_time_ns = *timestamp;
          std::invoke(consumer, std::move(value));
        }
      }
    } catch (const std::exception&) {
      ++metrics.parse_errors;
    }
  }
  if (!metadata_seen) {
    throw std::runtime_error("capture has no metadata record");
  }
  return metrics;
}

template <typename Queue>
replay_report replay_with(const std::filesystem::path& input,
                          std::string engine_name) {
  const auto start = now_ns();
  pipeline<Queue> runner;
  const auto metrics =
      read_capture(input, [&runner](trade value) {
        runner.submit(std::move(value));
      });
  runner.finish();
  return runner.report(
      std::move(engine_name), metrics, now_ns() - start);
}

json report_value(const replay_report& report) {
  return {
      {"schema_version", 1},
      {"bengal_version", bengal::version},
      {"engine", report.engine},
      {"input",
       {{"frames", report.input.frames},
        {"parse_errors", report.input.parse_errors},
        {"sequence_gaps", report.input.sequence_gaps},
        {"out_of_order", report.input.out_of_order}}},
      {"pipeline",
       {{"events", report.events},
        {"dropped", report.dropped},
        {"backpressure", report.backpressure},
        {"checksum", report.checksum},
        {"elapsed_ns", report.elapsed_ns},
        {"latency_ns",
         {{"samples", report.stage_latency.samples},
          {"min", report.stage_latency.minimum_ns},
          {"p50", report.stage_latency.p50_ns},
          {"p95", report.stage_latency.p95_ns},
          {"p99", report.stage_latency.p99_ns},
          {"max", report.stage_latency.maximum_ns}}}}}};
}

}  // namespace

std::optional<std::int64_t> parse_decimal(
    std::string_view value) noexcept {
  if (value.empty()) {
    return std::nullopt;
  }
  bool negative = false;
  if (value.front() == '-' || value.front() == '+') {
    negative = value.front() == '-';
    value.remove_prefix(1);
  }
  if (value.empty()) {
    return std::nullopt;
  }

  const auto dot = value.find('.');
  const auto whole_text = value.substr(0, dot);
  const auto fraction_text =
      dot == std::string_view::npos
          ? std::string_view{}
          : value.substr(dot + 1);
  if (whole_text.empty() || fraction_text.size() > 8 ||
      (dot != std::string_view::npos &&
       fraction_text.find('.') != std::string_view::npos)) {
    return std::nullopt;
  }

  std::uint64_t whole = 0;
  const auto whole_result =
      std::from_chars(whole_text.data(),
                      whole_text.data() + whole_text.size(),
                      whole);
  if (whole_result.ec != std::errc{} ||
      whole_result.ptr != whole_text.data() + whole_text.size()) {
    return std::nullopt;
  }
  std::uint64_t fraction = 0;
  for (const char character : fraction_text) {
    if (character < '0' || character > '9') {
      return std::nullopt;
    }
    fraction =
        fraction * 10 + static_cast<unsigned>(character - '0');
  }
  for (std::size_t index = fraction_text.size(); index < 8; ++index) {
    fraction *= 10;
  }

  const auto limit =
      negative
          ? static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()) +
                1
          : static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max());
  const auto scale = static_cast<std::uint64_t>(decimal_scale);
  if (whole > limit / scale) {
    return std::nullopt;
  }
  const auto scaled = whole * scale;
  if (fraction > limit - scaled) {
    return std::nullopt;
  }
  const auto magnitude = scaled + fraction;
  if (negative && magnitude == limit) {
    return std::numeric_limits<std::int64_t>::min();
  }
  const auto signed_value = static_cast<std::int64_t>(magnitude);
  return negative ? -signed_value : signed_value;
}

std::string format_decimal(std::int64_t value) {
  const bool negative = value < 0;
  const auto magnitude =
      negative
          ? std::uint64_t{0} - static_cast<std::uint64_t>(value)
          : static_cast<std::uint64_t>(value);
  const auto scale = static_cast<std::uint64_t>(decimal_scale);
  const auto whole = magnitude / scale;
  const auto fraction = magnitude % scale;
  std::ostringstream output;
  if (negative) {
    output << '-';
  }
  output << whole;
  if (fraction != 0) {
    output << '.' << std::setw(8) << std::setfill('0') << fraction;
    auto result = output.str();
    while (result.back() == '0') {
      result.pop_back();
    }
    return result;
  }
  return output.str();
}

replay_report replay(const std::filesystem::path& input,
                     pipeline_engine engine) {
  if (engine == pipeline_engine::bengal) {
    return replay_with<
        bengal::spsc_queue<trade, queue_capacity>>(input, "bengal");
  }
  return replay_with<
      standard_spsc_queue<trade, queue_capacity>>(input, "standard");
}

void generate_fixture(const std::filesystem::path& output,
                      std::uint64_t event_count,
                      std::string_view product_id) {
  if (event_count == 0) {
    throw std::invalid_argument("event count must be positive");
  }
  if (product_id.empty() || product_id.size() > 20) {
    throw std::invalid_argument(
        "product ID must contain 1 to 20 characters");
  }
  std::ofstream stream(output, std::ios::trunc);
  if (!stream) {
    throw std::runtime_error(
        "cannot create fixture: " + output.string());
  }
  stream
      << json{{"type", "metadata"},
              {"format", "bengal-market-capture"},
              {"version", 1},
              {"source", "deterministic-generator"},
              {"product_ids", json::array({product_id})}}
             .dump()
      << '\n';
  for (std::uint64_t index = 0; index < event_count; ++index) {
    const auto price =
        std::int64_t{3'000'000'000'000} +
        static_cast<std::int64_t>((index % 10'000) * 10'000);
    const auto amount =
        std::int64_t{100'000} +
        static_cast<std::int64_t>((index % 100) * 1'000);
    const json input_trade = {
        {"trade_id", std::to_string(index + 1)},
        {"product_id", product_id},
        {"price", format_decimal(price)},
        {"size", format_decimal(amount)},
        {"side", index % 2 == 0 ? "BUY" : "SELL"},
        {"time", "2026-01-01T00:00:00.000000000Z"}};
    const json event = {
        {"type", "update"},
        {"trades", json::array({input_trade})}};
    const json payload = {
        {"channel", "market_trades"},
        {"timestamp", "2026-01-01T00:00:00.000000000Z"},
        {"sequence_num", index + 1},
        {"events", json::array({event})}};
    stream
        << json{{"type", "frame"},
                {"received_ns", index * 1'000},
                {"connection_id", 1},
                {"payload", payload.dump()}}
               .dump()
        << '\n';
  }
  if (!stream) {
    throw std::runtime_error("failed writing fixture");
  }
}

std::string report_json(const replay_report& report) {
  return report_value(report).dump(2);
}

std::string comparison_json(const replay_report& bengal,
                            const replay_report& standard) {
  return json{
      {"schema_version", 1},
      {"comparable", reports_comparable(bengal, standard)},
      {"bengal", report_value(bengal)},
      {"standard", report_value(standard)}}
      .dump(2);
}

bool reports_comparable(const replay_report& bengal,
                        const replay_report& standard) noexcept {
  return bengal.input.frames == standard.input.frames &&
         bengal.input.parse_errors == 0 &&
         standard.input.parse_errors == 0 &&
         bengal.events == standard.events &&
         bengal.checksum == standard.checksum &&
         bengal.dropped == 0 && standard.dropped == 0 &&
         bengal.stage_latency.samples == bengal.events &&
         standard.stage_latency.samples == standard.events;
}

}  // namespace bengal_market
