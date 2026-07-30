#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace bengal_market {

inline constexpr std::size_t queue_capacity = 4096;
inline constexpr std::int64_t decimal_scale = 100'000'000;

template <std::size_t Capacity>
class bounded_text {
 public:
  bool assign(std::string_view value) noexcept {
    if (value.size() > Capacity) {
      return false;
    }
    size_ = value.size();
    for (std::size_t index = 0; index < size_; ++index) {
      data_[index] = value[index];
    }
    return true;
  }

  std::string_view view() const noexcept {
    return {data_.data(), size_};
  }

 private:
  std::array<char, Capacity> data_{};
  std::size_t size_{0};
};

enum class trade_side { buy, sell };

struct trade {
  bounded_text<24> trade_id;
  bounded_text<20> product_id;
  std::int64_t price{0};
  std::int64_t size{0};
  trade_side side{trade_side::buy};
  std::uint64_t source_time_ns{0};
  std::uint64_t enqueued_ns{0};
};

struct parse_metrics {
  std::uint64_t frames{0};
  std::uint64_t parse_errors{0};
  std::uint64_t sequence_gaps{0};
  std::uint64_t out_of_order{0};
};

struct latency_summary {
  std::uint64_t samples{0};
  std::uint64_t minimum_ns{0};
  std::uint64_t p50_ns{0};
  std::uint64_t p95_ns{0};
  std::uint64_t p99_ns{0};
  std::uint64_t maximum_ns{0};
};

struct replay_report {
  std::string engine;
  parse_metrics input;
  latency_summary stage_latency;
  std::uint64_t events{0};
  std::uint64_t dropped{0};
  std::uint64_t backpressure{0};
  std::uint64_t checksum{0};
  std::uint64_t elapsed_ns{0};
};

enum class pipeline_engine { bengal, standard };

std::optional<std::int64_t> parse_decimal(std::string_view value) noexcept;
std::string format_decimal(std::int64_t value);
replay_report replay(const std::filesystem::path& input, pipeline_engine engine);
void generate_fixture(const std::filesystem::path& output,
                      std::uint64_t event_count,
                      std::string_view product_id);
std::string report_json(const replay_report& report);
std::string comparison_json(const replay_report& bengal,
                            const replay_report& standard);
bool reports_comparable(const replay_report& bengal,
                        const replay_report& standard) noexcept;

}  // namespace bengal_market
