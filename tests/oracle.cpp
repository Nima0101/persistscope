#include <persistscope/persistscope.hpp>

#include <array>
#include <iostream>
#include <set>

// Independent oracle for a useful model subset: after each cut, each inode can
// hold its last synced value or any later write, independently of other inodes.
// This does not use event IDs, dependency closure, or the explorer algorithm.
int main() {
  namespace ps = persistscope;
  constexpr std::size_t length = 5;
  for (std::size_t code = 0; code < 1024; ++code) {
    ps::Scenario scenario{{{"a", "0"}, {"b", "0"}}, {}, {}};
    std::array<std::set<std::string>, length + 1> expected;
    std::array<std::set<std::string>, 2> possible{{{"0"}, {"0"}}};
    std::array<std::string, 2> current{{"0", "0"}};
    auto remaining = code;
    for (std::size_t cut = 0; cut <= length; ++cut) {
      for (const auto &a : possible[0])
        for (const auto &b : possible[1])
          expected[cut].insert(a + ":" + b);
      if (cut == length)
        break;
      const auto op = remaining % 4;
      remaining /= 4;
      const auto inode = op % 2;
      const auto name = inode == 0 ? "a" : "b";
      if (op < 2) {
        current[inode] = std::to_string(cut + 1);
        possible[inode].insert(current[inode]);
        scenario.run.push_back({ps::Kind::write, name, current[inode]});
      } else {
        possible[inode] = {current[inode]};
        scenario.run.push_back({ps::Kind::sync_file, name, ""});
      }
    }
    std::array<std::set<std::string>, length + 1> actual;
    const auto result = ps::check(
        scenario,
        [&](const ps::Snapshot &files, const ps::Context &context) -> std::optional<std::string> {
          actual[context.cut].insert(files.at("a") + ":" + files.at("b"));
          return std::nullopt;
        });
    if (result.status != ps::Status::pass || actual != expected) {
      std::cerr << "Oracle disagreement for protocol " << code << '\n';
      return 1;
    }
  }
  std::cout << "1024 write/sync protocols agree with independent Cartesian oracle\n";
}
