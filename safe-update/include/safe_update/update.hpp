#pragma once
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
namespace safe_update {
using Bytes = std::span<const unsigned char>;
using Fault = std::function<void(std::string_view)>;
std::string digest(std::string_view payload);
std::string manifest(std::string_view payload);
// Single owner of a trusted, pre-created directory. Throws on all I/O/verification failures.
class Store {
public:
  explicit Store(std::filesystem::path root, Fault fault = {});
  ~Store();
  Store(const Store &) = delete;
  Store &operator=(const Store &) = delete;
  void stage(std::string_view payload, std::string_view signed_manifest, Bytes signature,
             Bytes public_key);
  std::string active() const;
  void confirm(const std::function<bool(std::string_view)> &health);
  void recover();

private:
  std::filesystem::path root_;
  Fault fault_;
  int lock_ = -1;
  std::pair<std::string, std::string> state() const;
  void save(const std::string &stable, const std::string &trial);
  void checkpoint(std::string_view name) const;
};
} // namespace safe_update
