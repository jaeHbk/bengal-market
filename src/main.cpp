#include <bengal_market/live.hpp>
#include <bengal_market/model.hpp>

#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using arguments = std::unordered_map<std::string, std::string>;

void usage(std::ostream& output) {
  output << "Usage:\n"
         << "  bengal-market generate --output FILE [--events N] [--product ID]\n"
         << "  bengal-market replay --input FILE [--engine bengal|standard]\n"
         << "  bengal-market compare --input FILE\n"
         << "  bengal-market capture --output FILE [--product ID] "
            "[--duration SECONDS]\n";
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

const std::string& required(const arguments& values,
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
      std::cout << "bengal-market 0.1.0\n";
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
      std::cout << bengal_market::report_json(
                       bengal_market::replay(required(options, "input"),
                                             engine))
                << '\n';
      return 0;
    }
    if (command == "compare") {
      reject_unknown(options, {"input"});
      const auto& input = required(options, "input");
      const auto bengal = bengal_market::replay(
          input, bengal_market::pipeline_engine::bengal);
      const auto standard = bengal_market::replay(
          input, bengal_market::pipeline_engine::standard);
      std::cout << bengal_market::comparison_json(bengal, standard) << '\n';
      return bengal.events == standard.events &&
                     bengal.checksum == standard.checksum
                 ? 0
                 : 1;
    }
    if (command == "capture") {
      reject_unknown(options, {"output", "product", "duration"});
#if defined(BENGAL_MARKET_HAS_LIVE)
      bengal_market::capture_options capture;
      capture.output = required(options, "output");
      capture.product_ids = {value_or(options, "product", "BTC-USD")};
      capture.duration =
          std::chrono::seconds(unsigned_value(options, "duration", 30));
      const auto result = bengal_market::capture_live(capture);
      std::cout << "{\"frames\":" << result.frames
                << ",\"reconnects\":" << result.reconnects << "}\n";
      return 0;
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
