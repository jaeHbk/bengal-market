#include <bengal_market/benchmark.hpp>
#include <bengal_market/live.hpp>
#include <bengal_market/model.hpp>
#include <bengal_market/version.hpp>
#include <nlohmann/json.hpp>

#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using arguments = std::unordered_map<std::string, std::string>;

volatile std::sig_atomic_t capture_signal = 0;

extern "C" void request_capture_stop(int signal) {
  if (capture_signal == 0) {
    capture_signal = signal;
  }
}

class capture_signal_scope {
 public:
  capture_signal_scope() {
    capture_signal = 0;
    struct sigaction action {};
    action.sa_handler = request_capture_stop;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGINT, &action, &previous_int_) != 0) {
      throw std::runtime_error("cannot install capture signal handlers");
    }
    int_installed_ = true;
    if (sigaction(SIGTERM, &action, &previous_term_) != 0) {
      (void)sigaction(SIGINT, &previous_int_, nullptr);
      int_installed_ = false;
      throw std::runtime_error("cannot install capture signal handlers");
    }
    term_installed_ = true;
  }

  capture_signal_scope(const capture_signal_scope&) = delete;
  capture_signal_scope& operator=(const capture_signal_scope&) = delete;

  ~capture_signal_scope() {
    if (term_installed_) {
      (void)sigaction(SIGTERM, &previous_term_, nullptr);
    }
    if (int_installed_) {
      (void)sigaction(SIGINT, &previous_int_, nullptr);
    }
  }

 private:
  struct sigaction previous_int_ {};
  struct sigaction previous_term_ {};
  bool int_installed_{false};
  bool term_installed_{false};
};

void usage(std::ostream& output) {
  output << "Usage:\n"
         << "  bengal-market generate --output FILE [--events N] [--product ID]\n"
         << "  bengal-market replay --input FILE [--engine bengal|standard]\n"
         << "  bengal-market compare --input FILE\n"
         << "  bengal-market benchmark --input FILE --output DIR "
            "[--runs N] [--warmup N]\n"
         << "  bengal-market capture --output FILE [--product ID] "
            "[--duration SECONDS] [--endpoint URL]\n";
}

arguments parse_arguments(int argc, char** argv, int start) {
  arguments result;
  for (int index = start; index < argc; index += 2) {
    if (index + 1 >= argc ||
        std::string_view(argv[index]).find("--") != 0) {
      throw std::invalid_argument("options must be --name value pairs");
    }
    const auto inserted =
        result.emplace(std::string(argv[index]).substr(2), argv[index + 1]);
    if (!inserted.second) {
      throw std::invalid_argument("duplicate option --" + inserted.first->first);
    }
  }
  return result;
}

std::string required(const arguments& values,
                     const std::string& name) {
  const auto found = values.find(name);
  if (found == values.end() || found->second.empty()) {
    throw std::invalid_argument("missing --" + name);
  }
  return found->second;
}

std::string value_or(const arguments& values,
                     const std::string& name,
                     std::string fallback) {
  const auto found = values.find(name);
  return found == values.end() ? std::move(fallback) : found->second;
}

std::uint64_t unsigned_value(const arguments& values,
                             const std::string& name,
                             std::uint64_t fallback) {
  const auto text = value_or(values, name, std::to_string(fallback));
  std::uint64_t result = 0;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} ||
      parsed.ptr != text.data() + text.size()) {
    throw std::invalid_argument("invalid --" + name);
  }
  return result;
}

void reject_unknown(const arguments& values,
                    const std::vector<std::string_view>& accepted) {
  for (const auto& [name, unused] : values) {
    (void)unused;
    bool known = false;
    for (const auto candidate : accepted) {
      known = known || name == candidate;
    }
    if (!known) {
      throw std::invalid_argument("unknown option --" + name);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 2 &&
        (std::string_view(argv[1]) == "--help" ||
         std::string_view(argv[1]) == "-h")) {
      usage(std::cout);
      return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
      std::cout << "bengal-market " << bengal_market::version << '\n';
      return 0;
    }
    if (argc < 2) {
      usage(std::cerr);
      return 2;
    }
    const std::string command = argv[1];
    const auto options = parse_arguments(argc, argv, 2);

    if (command == "generate") {
      reject_unknown(options, {"output", "events", "product"});
      bengal_market::generate_fixture(
          required(options, "output"),
          unsigned_value(options, "events", 10'000),
          value_or(options, "product", "BTC-USD"));
      return 0;
    }
    if (command == "replay") {
      reject_unknown(options, {"input", "engine"});
      const auto engine_name = value_or(options, "engine", "bengal");
      bengal_market::pipeline_engine engine;
      if (engine_name == "bengal") {
        engine = bengal_market::pipeline_engine::bengal;
      } else if (engine_name == "standard") {
        engine = bengal_market::pipeline_engine::standard;
      } else {
        throw std::invalid_argument("--engine must be bengal or standard");
      }
      const auto report =
          bengal_market::replay(required(options, "input"), engine);
      std::cout << bengal_market::report_json(report) << '\n';
      return report.input.parse_errors == 0 && report.dropped == 0
                 ? 0
                 : 1;
    }
    if (command == "compare") {
      reject_unknown(options, {"input"});
      const auto& input = required(options, "input");
      const auto bengal = bengal_market::replay(
          input, bengal_market::pipeline_engine::bengal);
      const auto standard = bengal_market::replay(
          input, bengal_market::pipeline_engine::standard);
      std::cout << bengal_market::comparison_json(bengal, standard) << '\n';
      return bengal_market::reports_comparable(bengal, standard)
                 ? 0
                 : 1;
    }
    if (command == "benchmark") {
      reject_unknown(options, {"input", "output", "runs", "warmup"});
      bengal_market::benchmark_options benchmark;
      benchmark.input = required(options, "input");
      benchmark.output = required(options, "output");
      benchmark.executable = std::filesystem::canonical("/proc/self/exe");
      benchmark.runs = unsigned_value(options, "runs", 10);
      benchmark.warmups = unsigned_value(options, "warmup", 1);
      const auto result = bengal_market::run_benchmark(benchmark);
      std::cout
          << nlohmann::json{
                 {"output", result.output.string()},
                 {"measured_processes", result.measured_processes},
                 {"comparable", result.comparable}}
                 .dump()
          << '\n';
      return result.comparable ? 0 : 1;
    }
    if (command == "capture") {
      reject_unknown(
          options, {"output", "product", "duration", "endpoint"});
#if defined(BENGAL_MARKET_HAS_LIVE)
      bengal_market::capture_options capture;
      capture.output = required(options, "output");
      capture.product_ids = {value_or(options, "product", "BTC-USD")};
      capture.duration =
          std::chrono::seconds(unsigned_value(options, "duration", 30));
      capture.endpoint = value_or(
          options,
          "endpoint",
          "wss://advanced-trade-ws.coinbase.com");
      capture_signal_scope signals;
      const auto result = bengal_market::capture_live(
          capture, [] { return capture_signal != 0; });
      const int signal = capture_signal;
      std::cout
          << nlohmann::json{
                 {"frames", result.frames},
                 {"reconnects", result.reconnects},
                 {"interrupted", result.interrupted || signal != 0},
                 {"signal", signal}}
                 .dump()
          << '\n';
      return signal == 0 ? 0 : 128 + signal;
#else
      throw std::runtime_error("live capture was disabled at build time");
#endif
    }

    usage(std::cerr);
    return 2;
  } catch (const std::exception& error) {
    std::cerr << "bengal-market: " << error.what() << '\n';
    return 2;
  }
}
