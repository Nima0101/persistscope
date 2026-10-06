#include <iostream>
#include <persistscope/persistscope.hpp>

int main() {
  namespace ps = persistscope;
  ps::Scenario scenario{{{"manifest", "v1"}},
                        {{ps::Kind::write, "manifest", "v2"},
                         {ps::Kind::sync_file, "manifest", ""},
                         {ps::Kind::ack, "", ""}},
                        {}};
  const auto result = ps::check(
      scenario,
      [](const ps::Snapshot &files, const ps::Context &context) -> std::optional<std::string> {
        if (context.acknowledged && files.at("manifest") != "v2")
          return "acknowledged manifest was lost";
        return std::nullopt;
      });
  std::cout << ps::status_name(result.status) << ": " << result.outcomes << " outcomes\n";
  return result.status == ps::Status::pass ? 0 : 1;
}
