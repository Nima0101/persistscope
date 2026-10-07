#include <openssl/evp.h>
#include <safe_update/update.hpp>

#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unistd.h>

namespace {
void require(bool value) {
  if (!value)
    throw std::runtime_error("safe-update consumer check failed");
}
struct Directory {
  std::filesystem::path path;
  Directory() {
    auto pattern = (std::filesystem::temp_directory_path() / "update-demo-XXXXXX").string();
    require(mkdtemp(pattern.data()) != nullptr);
    path = pattern;
  }
  ~Directory() { std::filesystem::remove_all(path); }
};
} // namespace

int main() {
  try {
    require(safe_update::manifest("abc") ==
            "safe-update-v1\nba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n");
    // This ephemeral signer is demonstration data, never a production trust anchor.
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519"), EVP_PKEY_free);
    std::array<unsigned char, 32> public_key{};
    auto size = public_key.size();
    require(key && EVP_PKEY_get_raw_public_key(key.get(), public_key.data(), &size) == 1 &&
            size == public_key.size());
    Directory directory;
    auto stage = [&](safe_update::Store &store, std::string_view payload) {
      auto manifest = safe_update::manifest(payload);
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),
                                                                      EVP_MD_CTX_free);
      require(context &&
              EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) == 1);
      std::array<unsigned char, 64> signature{};
      auto length = signature.size();
      require(EVP_DigestSign(context.get(), signature.data(), &length,
                             reinterpret_cast<const unsigned char *>(manifest.data()),
                             manifest.size()) == 1 &&
              length == signature.size());
      store.stage(payload, manifest, signature, public_key);
    };
    {
      safe_update::Store store(directory.path);
      stage(store, "healthy-v1");
      store.confirm([](auto payload) { return payload == "healthy-v1"; });
      stage(store, "unhealthy-v2");
      store.confirm([](auto payload) { return payload == "healthy-v2"; });
      require(store.active() == "healthy-v1");
      stage(store, "healthy-v2");
      // Close without confirmation: startup recovery must select the stable payload.
    }
    safe_update::Store restarted(directory.path);
    restarted.recover();
    require(restarted.active() == "healthy-v1");
    stage(restarted, "healthy-v2");
    restarted.confirm([](auto payload) { return payload == "healthy-v2"; });
    require(restarted.active() == "healthy-v2");
    std::cout
        << "Signed staging, synthetic health rejection, restart rollback and confirmation passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
