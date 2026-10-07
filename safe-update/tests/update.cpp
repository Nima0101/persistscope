#include "safe_update/update.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
namespace {
void check(bool ok) {
  if (!ok)
    throw std::runtime_error("test assertion failed");
}
template <class F> void rejected(F action) {
  bool failed = false;
  try {
    action();
  } catch (const std::exception &) {
    failed = true;
  }
  check(failed);
}
struct Signer {
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key{
      EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519"), EVP_PKEY_free};
  std::array<unsigned char, 32> pub{};
  Signer() {
    std::size_t n = pub.size();
    check(key && EVP_PKEY_get_raw_public_key(key.get(), pub.data(), &n) == 1 && n == pub.size());
  }
  std::array<unsigned char, 64> sign(std::string_view text) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    check(ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1);
    std::array<unsigned char, 64> sig{};
    std::size_t n = sig.size();
    check(EVP_DigestSign(ctx.get(), sig.data(), &n,
                         reinterpret_cast<const unsigned char *>(text.data()), text.size()) == 1 &&
          n == sig.size());
    return sig;
  }
  void stage(safe_update::Store &store, std::string_view payload) {
    auto text = safe_update::manifest(payload);
    store.stage(payload, text, sign(text), pub);
  }
};
struct Temp {
  std::filesystem::path path;
  Temp() {
    auto base = (std::filesystem::temp_directory_path() / "safe-update-XXXXXX").string();
    check(mkdtemp(base.data()) != nullptr);
    path = base;
  }
  ~Temp() { std::filesystem::remove_all(path); }
};
void initialize(const std::filesystem::path &root, Signer &signer) {
  safe_update::Store store(root);
  signer.stage(store, "old");
  store.confirm([](auto bytes) { return bytes == "old"; });
}
} // namespace
int main() {
  try {
    Signer signer;
    Temp root;
    initialize(root.path, signer);
    {
      safe_update::Store store(root.path);
      rejected([&] { safe_update::Store second(root.path); });
      auto text = safe_update::manifest("new");
      auto sig = signer.sign(text);
      rejected([&] { store.stage("tampered", text, sig, signer.pub); });
      auto bad = sig;
      bad[0] ^= 1;
      rejected([&] { store.stage("new", text, bad, signer.pub); });
      Signer stranger;
      rejected([&] { store.stage("new", text, sig, stranger.pub); });
      rejected([&] { store.stage("new", "safe-update-v2\n", sig, signer.pub); });
      rejected([&] { store.stage(std::string(1024 * 1024 + 1, 'x'), text, sig, signer.pub); });
      check(store.active() == "old");
      signer.stage(store, "new");
      check(store.active() == "new");
      rejected([&] { signer.stage(store, "other"); });
      store.confirm([](auto) { return false; });
      check(store.active() == "old");
      signer.stage(store, "new");
      rejected(
          [&] { store.confirm([](auto) -> bool { throw std::runtime_error("health crashed"); }); });
      store.recover();
      check(store.active() == "old");
      signer.stage(store, "new");
      store.confirm([](auto value) { return value == "new"; });
      check(store.active() == "new");
      store.recover();
      check(store.active() == "new");
    }
    for (const auto *point : {"payload_file_synced", "payload_dir_synced", "state_file_synced",
                              "state_renamed", "state_dir_synced", "trial_ready"}) {
      Temp crash;
      initialize(crash.path, signer);
      auto child = fork();
      check(child >= 0);
      if (child == 0) {
        safe_update::Store store(crash.path, [&](auto event) {
          if (event == point)
            _exit(73);
        });
        signer.stage(store, "new");
        _exit(74);
      }
      int status = 0;
      check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73);
      safe_update::Store store(crash.path);
      store.recover();
      check(store.active() == "old");
      store.recover();
      check(store.active() == "old");
    }
    for (const auto *point :
         {"health_checked", "state_file_synced", "state_renamed", "state_dir_synced"}) {
      Temp crash;
      initialize(crash.path, signer);
      {
        safe_update::Store store(crash.path);
        signer.stage(store, "new");
      }
      auto child = fork();
      check(child >= 0);
      if (child == 0) {
        safe_update::Store store(crash.path, [&](auto event) {
          if (event == point)
            _exit(73);
        });
        store.confirm([](auto) { return true; });
        _exit(74);
      }
      int status = 0;
      check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73);
      safe_update::Store store(crash.path);
      store.recover();
      check(store.active() == (std::string_view(point) == "state_renamed" ||
                                       std::string_view(point) == "state_dir_synced"
                                   ? "new"
                                   : "old"));
    }
    {
      safe_update::Store store(root.path);
      std::ofstream(root.path / safe_update::digest("new")) << "corrupt";
      rejected([&] { store.active(); });
    }
    {
      Temp bad;
      std::filesystem::create_symlink(root.path / "state", bad.path / "state");
      safe_update::Store store(bad.path);
      rejected([&] { store.active(); });
    }
    {
      Temp bad;
      std::filesystem::create_directory(bad.path / "payload.tmp");
      safe_update::Store store(bad.path);
      rejected([&] { signer.stage(store, "new"); });
    }
    std::cout << "signature/hash negatives, health rollback, locking, I/O rejection and 10 process "
                 "interruption boundaries passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
