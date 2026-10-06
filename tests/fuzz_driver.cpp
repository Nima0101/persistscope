#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *, std::size_t);

int main() {
  std::uint32_t state = 0x1539ab21U;
  std::array<std::uint8_t, 64> bytes{};
  for (std::size_t sample = 0; sample < 10000; ++sample) {
    for (auto &byte : bytes) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      byte = static_cast<std::uint8_t>(state & 0xffU);
    }
    LLVMFuzzerTestOneInput(bytes.data(), sample % (bytes.size() + 1));
  }
  std::cout << "10000 deterministic generated programs checked; failure witnesses replayed\n";
}
