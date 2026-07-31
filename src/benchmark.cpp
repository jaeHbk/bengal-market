#include <bengal_market/benchmark.hpp>

#include <bengal/version.hpp>
#include <bengal_market/build_info.hpp>
#include <bengal_market/version.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace bengal_market {
namespace {

using json = nlohmann::json;
constexpr std::uint64_t maximum_runs = 1'000;
constexpr std::uint64_t maximum_warmups = 100;

struct process_result {
  int exit_code{0};
};

struct measured_run {
  std::uint64_t iteration{0};
  std::uint64_t order{0};
  std::string engine;
  std::filesystem::path output;
  std::filesystem::path error;
  json report;
};

std::string read_text(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("cannot read file: " + path.string());
  }
  std::ostringstream value;
  value << stream.rdbuf();
  if (!stream.eof() && stream.fail()) {
    throw std::runtime_error("failed reading file: " + path.string());
  }
  return value.str();
}

void write_text(const std::filesystem::path& path,
                std::string_view value) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("cannot create file: " + path.string());
  }
  stream.write(value.data(), static_cast<std::streamsize>(value.size()));
  if (!stream) {
    throw std::runtime_error("failed writing file: " + path.string());
  }
}

process_result run_process(const std::filesystem::path& executable,
                           const std::vector<std::string>& arguments,
                           const std::filesystem::path& standard_output,
                           const std::filesystem::path& standard_error) {
  const pid_t child = ::fork();
  if (child < 0) {
    throw std::runtime_error("fork failed");
  }
  if (child == 0) {
    const int output_fd =
        ::open(standard_output.c_str(),
               O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    const int error_fd =
        ::open(standard_error.c_str(),
               O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (output_fd < 0 || error_fd < 0 ||
        ::dup2(output_fd, STDOUT_FILENO) < 0 ||
        ::dup2(error_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    (void)::close(output_fd);
    (void)::close(error_fd);
    (void)::setenv("LC_ALL", "C", 1);

    std::vector<char*> values;
    values.reserve(arguments.size() + 2);
    auto executable_text = executable.string();
    values.push_back(executable_text.data());
    std::vector<std::string> owned = arguments;
    for (auto& argument : owned) {
      values.push_back(argument.data());
    }
    values.push_back(nullptr);
    ::execv(executable.c_str(), values.data());
    _exit(127);
  }

  int status = 0;
  for (;;) {
    const auto waited = ::waitpid(child, &status, 0);
    if (waited == child) {
      break;
    }
    if (waited < 0 && errno == EINTR) {
      continue;
    }
    throw std::runtime_error("waitpid failed");
  }
  if (WIFEXITED(status)) {
    return {WEXITSTATUS(status)};
  }
  if (WIFSIGNALED(status)) {
    return {128 + WTERMSIG(status)};
  }
  return {125};
}

std::string sha256(const std::filesystem::path& input,
                   const std::filesystem::path& working) {
  const auto output = working / "sha256.stdout";
  const auto error = working / "sha256.stderr";
  const auto result =
      run_process("/usr/bin/sha256sum", {input.string()}, output, error);
  if (result.exit_code != 0) {
    throw std::runtime_error(
        "sha256sum failed: " + read_text(error));
  }
  const auto text = read_text(output);
  std::filesystem::remove(output);
  std::filesystem::remove(error);
  if (text.size() < 64 ||
      !std::all_of(text.begin(), text.begin() + 64, [](char value) {
        return (value >= '0' && value <= '9') ||
               (value >= 'a' && value <= 'f');
      })) {
    throw std::runtime_error("sha256sum returned an invalid digest");
  }
  return text.substr(0, 64);
}

std::string trim(std::string value) {
  while (!value.empty() &&
         (value.back() == '\n' || value.back() == '\r' ||
          value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }
  const auto first = value.find_first_not_of(" \t");
  return first == std::string::npos ? std::string{} : value.substr(first);
}

std::optional<std::string> first_matching_line(
    const std::filesystem::path& path,
    std::string_view prefix) {
  std::ifstream stream(path);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.starts_with(prefix)) {
      const auto separator = line.find(':');
      return trim(separator == std::string::npos
                      ? line
                      : line.substr(separator + 1));
    }
  }
  return std::nullopt;
}

std::uint64_t memory_bytes() {
  const auto value = first_matching_line("/proc/meminfo", "MemTotal:");
  if (!value) {
    return 0;
  }
  std::uint64_t kibibytes = 0;
  std::istringstream input(*value);
  input >> kibibytes;
  if (!input || kibibytes >
                    std::numeric_limits<std::uint64_t>::max() / 1024) {
    return 0;
  }
  return kibibytes * 1024;
}

std::string read_optional(const std::filesystem::path& path) {
  std::ifstream stream(path);
  std::string value;
  std::getline(stream, value);
  return stream ? trim(std::move(value)) : "unknown";
}

std::string utc_now() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm value{};
  if (::gmtime_r(&time, &value) == nullptr) {
    return "unknown";
  }
  std::ostringstream output;
  output << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

json environment_value() {
  utsname system{};
  if (::uname(&system) != 0) {
    throw std::runtime_error("uname failed");
  }
#if defined(__clang__)
  constexpr std::string_view compiler = "Clang " __clang_version__;
#elif defined(__GNUC__)
  constexpr std::string_view compiler = "GCC " __VERSION__;
#else
  constexpr std::string_view compiler = "unknown";
#endif
  return {
      {"operating_system", system.sysname},
      {"kernel_release", system.release},
      {"architecture", system.machine},
      {"cpu_model",
       first_matching_line("/proc/cpuinfo", "model name")
           .value_or("unknown")},
      {"logical_cpus", std::thread::hardware_concurrency()},
      {"memory_bytes", memory_bytes()},
      {"scaling_governor",
       read_optional(
           "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")},
      {"compiler", compiler}};
}

std::uint64_t metric(const measured_run& run,
                     std::string_view group,
                     std::string_view name) {
  return run.report.at(group).at(name).get<std::uint64_t>();
}

std::uint64_t latency_metric(const measured_run& run,
                             std::string_view name) {
  return run.report.at("pipeline")
      .at("latency_ns")
      .at(name)
      .get<std::uint64_t>();
}

std::uint64_t nearest_rank(const std::vector<std::uint64_t>& sorted,
                           std::uint64_t percentile) {
  if (sorted.empty()) {
    return 0;
  }
  const auto rank =
      (static_cast<std::uint64_t>(sorted.size()) * percentile + 99) /
      100;
  return sorted[static_cast<std::size_t>(
      std::max<std::uint64_t>(1, rank) - 1)];
}

json distribution(std::vector<std::uint64_t> values) {
  std::sort(values.begin(), values.end());
  return {
      {"samples", values.size()},
      {"min", values.empty() ? 0 : values.front()},
      {"p25", nearest_rank(values, 25)},
      {"median", nearest_rank(values, 50)},
      {"p75", nearest_rank(values, 75)},
      {"p95", nearest_rank(values, 95)},
      {"max", values.empty() ? 0 : values.back()}};
}

json summarize(const std::vector<measured_run>& runs,
               std::string_view engine) {
  std::vector<std::uint64_t> elapsed;
  std::vector<std::uint64_t> p50;
  std::vector<std::uint64_t> p95;
  std::vector<std::uint64_t> p99;
  std::vector<std::uint64_t> backpressure;
  for (const auto& run : runs) {
    if (run.engine != engine) {
      continue;
    }
    elapsed.push_back(metric(run, "pipeline", "elapsed_ns"));
    p50.push_back(latency_metric(run, "p50"));
    p95.push_back(latency_metric(run, "p95"));
    p99.push_back(latency_metric(run, "p99"));
    backpressure.push_back(
        metric(run, "pipeline", "backpressure"));
  }
  return {
      {"elapsed_ns", distribution(std::move(elapsed))},
      {"stage_latency_p50_ns", distribution(std::move(p50))},
      {"stage_latency_p95_ns", distribution(std::move(p95))},
      {"stage_latency_p99_ns", distribution(std::move(p99))},
      {"backpressure", distribution(std::move(backpressure))}};
}

bool comparable(const std::vector<measured_run>& runs) {
  if (runs.empty()) {
    return false;
  }
  const auto& first = runs.front().report;
  const auto expected_events =
      first.at("pipeline").at("events").get<std::uint64_t>();
  const auto expected_checksum =
      first.at("pipeline").at("checksum").get<std::uint64_t>();
  const auto expected_frames =
      first.at("input").at("frames").get<std::uint64_t>();
  const auto expected_gaps =
      first.at("input").at("sequence_gaps").get<std::uint64_t>();
  const auto expected_out_of_order =
      first.at("input").at("out_of_order").get<std::uint64_t>();
  for (const auto& run : runs) {
    const auto& report = run.report;
    if (report.at("input").at("parse_errors").get<std::uint64_t>() != 0 ||
        report.at("pipeline").at("dropped").get<std::uint64_t>() != 0 ||
        report.at("pipeline").at("events").get<std::uint64_t>() !=
            expected_events ||
        report.at("pipeline").at("checksum").get<std::uint64_t>() !=
            expected_checksum ||
        report.at("input").at("frames").get<std::uint64_t>() !=
            expected_frames ||
        report.at("input").at("sequence_gaps").get<std::uint64_t>() !=
            expected_gaps ||
        report.at("input").at("out_of_order").get<std::uint64_t>() !=
            expected_out_of_order ||
        report.at("pipeline")
                .at("latency_ns")
                .at("samples")
                .get<std::uint64_t>() != expected_events) {
      return false;
    }
  }
  return true;
}

std::string html_escape(std::string_view input) {
  std::string output;
  output.reserve(input.size());
  for (const char value : input) {
    switch (value) {
      case '&':
        output += "&amp;";
        break;
      case '<':
        output += "&lt;";
        break;
      case '>':
        output += "&gt;";
        break;
      case '"':
        output += "&quot;";
        break;
      default:
        output += value;
        break;
    }
  }
  return output;
}

std::string html_report(const json& manifest) {
  const auto& bengal = manifest.at("summary").at("bengal");
  const auto& standard = manifest.at("summary").at("standard");
  const auto row = [](std::string_view label,
                      const json& left,
                      const json& right) {
    std::ostringstream value;
    value << "<tr><th>" << label << "</th><td>"
          << left.at("median") << "</td><td>" << left.at("p25")
          << " - " << left.at("p75") << "</td><td>"
          << right.at("median") << "</td><td>" << right.at("p25")
          << " - " << right.at("p75") << "</td></tr>";
    return value.str();
  };

  std::ostringstream output;
  output
      << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      << "<meta name=\"viewport\" content=\"width=device-width,"
         "initial-scale=1\">"
      << "<title>Bengal Market benchmark report</title><style>"
      << "body{font:15px system-ui,sans-serif;max-width:960px;margin:40px "
         "auto;padding:0 20px;color:#171717}table{border-collapse:collapse;"
         "width:100%}th,td{border:1px solid #bbb;padding:8px;text-align:right}"
         "th:first-child{text-align:left}code{background:#eee;padding:2px 4px}"
         ".ok{color:#126b2f}.bad{color:#9b1c1c}</style></head><body>"
      << "<h1>Bengal Market benchmark report</h1><p>Fixture: <code>"
      << html_escape(
             manifest.at("fixture").at("sha256").get<std::string>())
      << "</code></p><p>Measured pairs: "
      << manifest.at("benchmark").at("runs")
      << "; warm-up pairs: "
      << manifest.at("benchmark").at("warmups") << "</p><p class=\""
      << (manifest.at("comparable").get<bool>() ? "ok" : "bad")
      << "\">Comparable: "
      << (manifest.at("comparable").get<bool>() ? "yes" : "no")
      << "</p><table><thead><tr><th>Metric</th>"
         "<th>Bengal median</th><th>Bengal p25 - p75</th>"
         "<th>Standard median</th><th>Standard p25 - p75</th>"
         "</tr></thead><tbody>"
      << row("Elapsed ns",
             bengal.at("elapsed_ns"),
             standard.at("elapsed_ns"))
      << row("Stage p50 ns",
             bengal.at("stage_latency_p50_ns"),
             standard.at("stage_latency_p50_ns"))
      << row("Stage p95 ns",
             bengal.at("stage_latency_p95_ns"),
             standard.at("stage_latency_p95_ns"))
      << row("Stage p99 ns",
             bengal.at("stage_latency_p99_ns"),
             standard.at("stage_latency_p99_ns"))
      << "</tbody></table><p>These are local bounded-pipeline measurements, "
         "not exchange-to-client latency or financial performance.</p>"
         "</body></html>\n";
  return output.str();
}

json parse_report(const std::filesystem::path& path,
                  std::string_view expected_engine) {
  const auto report = json::parse(read_text(path));
  if (report.at("schema_version") != 1 ||
      report.at("engine") != expected_engine) {
    throw std::runtime_error(
        "child replay returned an incompatible report: " + path.string());
  }
  return report;
}

}  // namespace

benchmark_result run_benchmark(const benchmark_options& options) {
  if (options.input.empty() || options.output.empty() ||
      options.executable.empty()) {
    throw std::invalid_argument(
        "benchmark input, output, and executable are required");
  }
  if (options.runs == 0 || options.runs > maximum_runs) {
    throw std::invalid_argument(
        "benchmark runs must be between 1 and 1000");
  }
  if (options.warmups > maximum_warmups) {
    throw std::invalid_argument(
        "benchmark warmups must be between 0 and 100");
  }
  const auto input = std::filesystem::canonical(options.input);
  const auto executable = std::filesystem::canonical(options.executable);
  const auto output = std::filesystem::absolute(options.output);
  const auto working =
      std::filesystem::path(output.string() + ".part");
  if (std::filesystem::exists(output) ||
      std::filesystem::exists(working)) {
    throw std::runtime_error(
        "benchmark output or partial output already exists");
  }
  std::filesystem::create_directories(working / "runs");

  const auto digest = sha256(input, working);
  const auto executable_digest = sha256(executable, working);
  write_text(working / "fixture.sha256",
             digest + "  " + input.filename().string() + "\n");

  const auto run_replay =
      [&](std::uint64_t iteration,
          std::uint64_t order,
          std::string engine,
          bool measured) -> std::optional<measured_run> {
    const auto stem =
        (measured ? "run-" : "warmup-") +
        [&] {
          std::ostringstream value;
          value << std::setfill('0') << std::setw(3) << iteration
                << "-order-" << std::setw(2) << order << "-" << engine;
          return value.str();
        }();
    const auto base = measured ? working / "runs" : working;
    const auto standard_output = base / (stem + ".json");
    const auto standard_error = base / (stem + ".stderr.txt");
    const auto result =
        run_process(executable,
                    {"replay",
                     "--input",
                     input.string(),
                     "--engine",
                     engine},
                    standard_output,
                    standard_error);
    if (result.exit_code > 1) {
      throw std::runtime_error(
          engine + " replay process failed with exit code " +
          std::to_string(result.exit_code) + ": " +
          read_text(standard_error));
    }
    const auto report = parse_report(standard_output, engine);
    if (!measured) {
      std::filesystem::remove(standard_output);
      std::filesystem::remove(standard_error);
      return std::nullopt;
    }
    measured_run run;
    run.iteration = iteration;
    run.order = order;
    run.engine = std::move(engine);
    run.output = std::filesystem::relative(standard_output, working);
    run.error = std::filesystem::relative(standard_error, working);
    run.report = std::move(report);
    return run;
  };

  for (std::uint64_t iteration = 1; iteration <= options.warmups;
       ++iteration) {
    const bool bengal_first = iteration % 2 != 0;
    (void)run_replay(iteration,
                     1,
                     bengal_first ? "bengal" : "standard",
                     false);
    (void)run_replay(iteration,
                     2,
                     bengal_first ? "standard" : "bengal",
                     false);
  }

  std::vector<measured_run> measured;
  measured.reserve(static_cast<std::size_t>(options.runs * 2));
  for (std::uint64_t iteration = 1; iteration <= options.runs;
       ++iteration) {
    const bool bengal_first = iteration % 2 != 0;
    measured.push_back(*run_replay(
        iteration, 1, bengal_first ? "bengal" : "standard", true));
    measured.push_back(*run_replay(
        iteration, 2, bengal_first ? "standard" : "bengal", true));
  }

  json run_index = json::array();
  for (const auto& run : measured) {
    run_index.push_back(
        {{"iteration", run.iteration},
         {"order", run.order},
         {"engine", run.engine},
         {"report", run.output.generic_string()},
         {"stderr", run.error.generic_string()}});
  }
  const bool valid = comparable(measured);
  const json manifest = {
      {"schema_version", 1},
      {"created_at_utc", utc_now()},
      {"tool",
       {{"name", "bengal-market"},
        {"version", version},
        {"source_revision", source_revision},
        {"bengal_version", bengal::version},
        {"executable_sha256", executable_digest}}},
      {"benchmark",
       {{"runs", options.runs},
        {"warmups", options.warmups},
        {"fresh_process_per_replay", true},
        {"engine_order", "alternating"}}},
      {"fixture",
       {{"name", input.filename().string()},
        {"size_bytes", std::filesystem::file_size(input)},
        {"sha256", digest}}},
      {"environment", environment_value()},
      {"comparable", valid},
      {"summary",
       {{"bengal", summarize(measured, "bengal")},
        {"standard", summarize(measured, "standard")}}},
      {"runs", std::move(run_index)},
      {"scope",
       "Local bounded-pipeline measurements only; not exchange-to-client "
       "latency or financial performance."}};
  write_text(working / "manifest.json", manifest.dump(2) + "\n");
  write_text(working / "report.html", html_report(manifest));
  std::filesystem::rename(working, output);

  return {output,
          static_cast<std::uint64_t>(measured.size()),
          valid};
}

}  // namespace bengal_market
