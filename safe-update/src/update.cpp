#include "safe_update/update.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace safe_update {
namespace {
constexpr std::size_t max_payload = 1024 * 1024;
void need(bool ok) {
  if (!ok)
    throw std::runtime_error("safe-update operation rejected");
}
struct Fd {
  int value;
  explicit Fd(int fd) : value(fd) { need(fd >= 0); }
  ~Fd() { close(value); }
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
};
void sync_dir(const std::filesystem::path &root) {
  Fd fd(open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  need(fsync(fd.value) == 0);
}
std::string read(const std::filesystem::path &path, std::size_t limit) {
  Fd fd(open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
  struct stat meta{};
  need(fstat(fd.value, &meta) == 0 && S_ISREG(meta.st_mode));
  std::string result;
  std::array<char, 4096> buffer{};
  for (;;) {
    auto n = ::read(fd.value, buffer.data(), buffer.size());
    if (n < 0 && errno == EINTR)
      continue;
    need(n >= 0);
    if (n == 0)
      break;
    need(result.size() + static_cast<std::size_t>(n) <= limit);
    result.append(buffer.data(), static_cast<std::size_t>(n));
  }
  return result;
}
void write(const std::filesystem::path &path, std::string_view value) {
  Fd fd(open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600));
  std::size_t offset = 0;
  while (offset < value.size()) {
    auto n = ::write(fd.value, value.data() + offset, value.size() - offset);
    if (n < 0 && errno == EINTR)
      continue;
    need(n > 0);
    offset += static_cast<std::size_t>(n);
  }
  need(fsync(fd.value) == 0);
}
bool hash_id(std::string_view text) {
  return text.size() == 64 && text.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}
} // namespace
std::string digest(std::string_view payload) {
  need(payload.size() <= max_payload);
  std::array<unsigned char, 32> bytes{};
  unsigned int length = 0;
  need(EVP_Digest(payload.data(), payload.size(), bytes.data(), &length, EVP_sha256(), nullptr) ==
           1 &&
       length == 32);
  std::string result;
  for (auto byte : bytes) {
    result.push_back("0123456789abcdef"[byte >> 4]);
    result.push_back("0123456789abcdef"[byte & 15]);
  }
  return result;
}
std::string manifest(std::string_view payload) {
  return "safe-update-v1\n" + digest(payload) + "\n";
}
Store::Store(std::filesystem::path root, Fault fault)
    : root_(std::move(root)), fault_(std::move(fault)) {
  need(std::filesystem::is_directory(std::filesystem::symlink_status(root_)));
  lock_ = open((root_ / "lock").c_str(), O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
  need(lock_ >= 0);
  if (flock(lock_, LOCK_EX | LOCK_NB) != 0) {
    close(lock_);
    lock_ = -1;
    need(false);
  }
}
Store::~Store() {
  if (lock_ >= 0)
    close(lock_);
}
void Store::checkpoint(std::string_view name) const {
  if (fault_)
    fault_(name);
}
std::pair<std::string, std::string> Store::state() const {
  if (!std::filesystem::exists(std::filesystem::symlink_status(root_ / "state")))
    return {"-", "-"};
  auto text = read(root_ / "state", 150);
  auto split = text.find('\n');
  need(split != std::string::npos);
  auto stable = text.substr(0, split);
  auto trial = text.substr(split + 1);
  need((stable == "-" || hash_id(stable)) && (trial == "-" || hash_id(trial)));
  return {stable, trial};
}
void Store::save(const std::string &stable, const std::string &trial) {
  write(root_ / "state.tmp", stable + "\n" + trial);
  checkpoint("state_file_synced");
  need(rename((root_ / "state.tmp").c_str(), (root_ / "state").c_str()) == 0);
  checkpoint("state_renamed");
  sync_dir(root_);
  checkpoint("state_dir_synced");
}
void Store::stage(std::string_view payload, std::string_view signed_manifest, Bytes signature,
                  Bytes public_key) {
  need(payload.size() <= max_payload && public_key.size() == 32 && signature.size() == 64);
  need(signed_manifest == manifest(payload));
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
      EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_key.data(), public_key.size()),
      EVP_PKEY_free);
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  need(key && ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1);
  need(EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                        reinterpret_cast<const unsigned char *>(signed_manifest.data()),
                        signed_manifest.size()) == 1);
  auto [stable, trial] = state();
  need(trial == "-");
  auto id = digest(payload);
  write(root_ / "payload.tmp", payload);
  checkpoint("payload_file_synced");
  need(rename((root_ / "payload.tmp").c_str(), (root_ / id).c_str()) == 0);
  sync_dir(root_);
  checkpoint("payload_dir_synced");
  save(stable, id);
  checkpoint("trial_ready");
}
std::string Store::active() const {
  auto [stable, trial] = state();
  auto id = trial == "-" ? stable : trial;
  need(id != "-");
  auto payload = read(root_ / id, max_payload);
  need(digest(payload) == id);
  return payload;
}
void Store::confirm(const std::function<bool(std::string_view)> &health) {
  auto [stable, trial] = state();
  need(trial != "-");
  bool healthy = health(active());
  checkpoint("health_checked");
  save(healthy ? trial : stable, "-");
}
void Store::recover() {
  auto [stable, trial] = state();
  if (trial != "-")
    save(stable, "-");
}
} // namespace safe_update
