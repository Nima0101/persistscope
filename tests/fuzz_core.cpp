#include <persistscope/persistscope.hpp>

#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
  namespace ps = persistscope;
  if (size > 64)
    return 0;
  ps::Scenario scenario{{{"a", "00"}, {"b", "00"}}, {}, {}};
  for (std::size_t i = 0; i + 2 < size; i += 3) {
    ps::Operation operation{static_cast<ps::Kind>(data[i] % 10), data[i + 1] % 2 == 0 ? "a" : "b",
                            ""};
    if (operation.kind == ps::Kind::write || operation.kind == ps::Kind::patch)
      operation.value = std::string(1, static_cast<char>(data[i + 2]));
    if (operation.kind == ps::Kind::rename || operation.kind == ps::Kind::rename_if_exists)
      operation.value = data[i + 2] % 2 == 0 ? "a" : "b";
    if (operation.kind == ps::Kind::ack || operation.kind == ps::Kind::sync_dir)
      operation.path.clear();
    auto &program = data[i] < 128 ? scenario.run : scenario.recovery;
    program.push_back(operation);
  }
  const ps::Invariant invariant = [](const ps::Snapshot &files,
                                     const ps::Context &context) -> std::optional<std::string> {
    if (context.acknowledged && (!files.contains("a") || files.at("a") != "00"))
      return "acknowledged data differs";
    return std::nullopt;
  };
  ps::Result result;
  try {
    result = ps::check(scenario, invariant, {1000, {}});
  } catch (const ps::Error &) {
    // Invalid generated programs are expected, including missing names.
    return 0;
  }
  if (result.witness) {
    const auto &witness = *result.witness;
    const auto replay = ps::replay(scenario, witness.cut, witness.durable_events, invariant);
    if (replay.crashed != witness.crashed || replay.recovered != witness.recovered ||
        replay.reason != witness.reason)
      std::abort();
  }
  return 0;
}
