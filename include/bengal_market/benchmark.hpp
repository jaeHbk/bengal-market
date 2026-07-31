#pragma once

#include <cstdint>
#include <filesystem>

namespace bengal_market {

struct benchmark_options {
  std::filesystem::path input;
  std::filesystem::path output;
  std::filesystem::path executable;
  std::uint64_t runs{10};
  std::uint64_t warmups{1};
};

struct benchmark_result {
  std::filesystem::path output;
  std::uint64_t measured_processes{0};
  bool comparable{false};
};

benchmark_result run_benchmark(const benchmark_options& options);

}  // namespace bengal_market
