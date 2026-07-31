#pragma once

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace bengal_market::detail {

class atomic_output {
 public:
  explicit atomic_output(std::filesystem::path destination)
      : destination_(std::move(destination)),
        temporary_(destination_.string() + ".part") {
    if (destination_.empty() || destination_.filename().empty()) {
      throw std::invalid_argument("output must name a file");
    }
    if (std::filesystem::exists(destination_)) {
      throw std::runtime_error(
          "output already exists: " + destination_.string());
    }
    fd_ = ::open(temporary_.c_str(),
                 O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd_ < 0) {
      throw_system_error("cannot create partial output", temporary_);
    }
  }

  atomic_output(const atomic_output&) = delete;
  atomic_output& operator=(const atomic_output&) = delete;

  ~atomic_output() {
    if (fd_ >= 0) {
      (void)::close(fd_);
    }
  }

  void write(std::string_view data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
      const auto result =
          ::write(fd_, data.data() + offset, data.size() - offset);
      if (result < 0 && errno == EINTR) {
        continue;
      }
      if (result <= 0) {
        throw_system_error("failed writing partial output", temporary_);
      }
      offset += static_cast<std::size_t>(result);
    }
  }

  void write_line(std::string_view line) {
    write(line);
    write("\n");
  }

  void commit() {
    if (committed_) {
      throw std::logic_error("output already committed");
    }
    if (::fsync(fd_) != 0) {
      throw_system_error("failed syncing partial output", temporary_);
    }
    if (::close(fd_) != 0) {
      fd_ = -1;
      throw_system_error("failed closing partial output", temporary_);
    }
    fd_ = -1;

    if (::link(temporary_.c_str(), destination_.c_str()) != 0) {
      throw_system_error("failed publishing output", destination_);
    }
    if (::unlink(temporary_.c_str()) != 0) {
      throw_system_error("published output but failed removing partial output",
                         temporary_);
    }
    sync_parent();
    committed_ = true;
  }

  const std::filesystem::path& temporary_path() const noexcept {
    return temporary_;
  }

 private:
  [[noreturn]] static void throw_system_error(
      std::string_view operation,
      const std::filesystem::path& path) {
    const int code = errno;
    throw std::runtime_error(
        std::string(operation) + ": " + path.string() + ": " +
        std::strerror(code));
  }

  void sync_parent() {
    auto parent = destination_.parent_path();
    if (parent.empty()) {
      parent = ".";
    }
    const int directory =
        ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) {
      throw_system_error("failed opening output directory", parent);
    }
    if (::fsync(directory) != 0) {
      const int code = errno;
      (void)::close(directory);
      errno = code;
      throw_system_error("failed syncing output directory", parent);
    }
    if (::close(directory) != 0) {
      throw_system_error("failed closing output directory", parent);
    }
  }

  std::filesystem::path destination_;
  std::filesystem::path temporary_;
  int fd_{-1};
  bool committed_{false};
};

}  // namespace bengal_market::detail
