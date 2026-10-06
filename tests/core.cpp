#include <persistscope/persistscope.hpp>

#include <future>
#include <iostream>
#include <limits>
#include <set>

using namespace persistscope;
namespace {
int failures = 0;
void require(bool condition, const char *name) {
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    ++failures;
  }
}
template <typename F> void rejects(F f, const char *name) {
  try {
    f();
    require(false, name);
  } catch (const Error &) {
  }
}
const Invariant ok = [](const Snapshot &, const Context &) { return std::nullopt; };
const Invariant replacement = [](const Snapshot &f,
                                 const Context &c) -> std::optional<std::string> {
  const auto it = f.find("config");
  if (it == f.end() || (it->second != "old" && it->second != "new"))
    return "corrupt";
  if (c.acknowledged && it->second != "new")
    return "lost acknowledged update";
  return std::nullopt;
};
Scenario replace(bool sync) {
  Scenario s{{{"config", "old"}},
             {{Kind::create, "tmp", ""},
              {Kind::write, "tmp", "new"},
              {Kind::sync_file, "tmp", ""},
              {Kind::rename, "tmp", "config"}},
             {}};
  if (sync)
    s.run.push_back({Kind::sync_dir, "", ""});
  s.run.push_back({Kind::ack, "", ""});
  return s;
}
} // namespace
int main() {
  const auto bad = check(replace(false), replacement);
  require(bad.status == Status::fail, "missing directory sync fails");
  if (bad.witness) {
    const auto w =
        replay(replace(false), bad.witness->cut, bad.witness->durable_events, replacement);
    require(w.reason == bad.witness->reason && w.recovered == bad.witness->recovered,
            "witness replay exact");
  } else
    require(false, "failure has witness");
  const auto good = check(replace(true), replacement);
  require(good.status == Status::pass && good.cuts_completed == 7,
          "synced replacement passes all cuts");
  Scenario s{{{"a", "00"}, {"b", "00"}}, {{Kind::patch, "a", "11", 0}}, {}};
  const auto torn = check(s, [](const Snapshot &f, const Context &) -> std::optional<std::string> {
    if (f.at("a") != "00" && f.at("a") != "11")
      return "torn";
    return std::nullopt;
  });
  require(torn.status == Status::fail && torn.witness->crashed.at("a") == "10", "byte patch tears");
  s.run = {{Kind::write, "a", "1"}, {Kind::write, "b", "1"}};
  std::set<std::string> outcomes;
  check(s, [&](const Snapshot &f, const Context &c) -> std::optional<std::string> {
    if (c.cut == 2)
      outcomes.insert(f.at("a") + ":" + f.at("b"));
    return std::nullopt;
  });
  require(outcomes.size() == 4, "independent inodes reorder");
  s.run = {{Kind::write, "a", "1"}, {Kind::write, "a", "2"}};
  require(check(s, ok).outcomes == 6, "same-inode writes form ordered chain");
  Scenario names{{}, {{Kind::create, "a", ""}, {Kind::create, "b", ""}}, {}};
  std::set<std::string> name_sets;
  check(names, [&](const Snapshot &f, const Context &c) -> std::optional<std::string> {
    if (c.cut == 2)
      name_sets.insert(std::string(f.contains("a") ? "a" : "") + (f.contains("b") ? "b" : ""));
    return std::nullopt;
  });
  require(name_sets == std::set<std::string>{"", "a", "b", "ab"},
          "namespace operations on different names reorder");
  Scenario identity{
      {{"a", "old"}},
      {{Kind::rename, "a", "b"}, {Kind::write, "b", "new"}, {Kind::sync_file, "b", ""}},
      {}};
  const auto wi = replay(identity, 3, {1}, ok);
  require(wi.crashed == Snapshot{{"a", "new"}}, "fsync by renamed path flushes original inode");
  Scenario recover{{{"a", "old"}, {"journal", "new"}},
                   {{Kind::rename, "journal", "a"}},
                   {{Kind::rename_if_exists, "journal", "a"}}};
  require(check(recover,
                [](const Snapshot &f, const Context &) -> std::optional<std::string> {
                  if (f.at("a") != "new")
                    return "recovery lost data";
                  return std::nullopt;
                }).status == Status::pass,
          "idempotent recovery at every cut");
  recover.recovery = {{Kind::rename, "journal", "a"}};
  require(check(recover, ok).status == Status::fail, "recovery errors are counterexamples");
  require(check(replace(true), replacement, {1, {}}).status == Status::incomplete,
          "node limit not pass");
  require(check(replace(true), replacement, {100, [] { return true; }}).nodes == 0,
          "immediate cancellation");
  rejects([&] { check(s, ok, {0, {}}); }, "zero budget");
  rejects([&] { check(s, ok, {10000001, {}}); }, "excessive budget");
  rejects([&] { replay(replace(true), 99, {}, ok); }, "invalid cut");
  rejects([&] { replay(replace(true), 6, {}, ok); }, "missing forced events");
  rejects([&] { replay(replace(false), 4, {2}, ok); }, "missing namespace predecessor");
  rejects([&] { replay(replace(false), 4, {0, 0}, ok); }, "duplicate event ids");
  rejects([&] { replay(replace(false), 0, {0}, ok); }, "event beyond cut");
  rejects([&] { check({{{"../escape", "x"}}, {}, {}}, ok); }, "unsafe virtual name");
  rejects([&] { check({{{"a", std::string(4097, 'x')}}, {}, {}}, ok); }, "oversized initial value");
  rejects([&] { check({{}, std::vector<Operation>(129, {Kind::ack, "", ""}), {}}, ok); },
          "oversized program");
  rejects(
      [&] {
        check(
            {{{"a", "x"}}, {{Kind::patch, "a", "x", std::numeric_limits<std::size_t>::max()}}, {}},
            ok);
      },
      "patch offset overflow");
  rejects(
      [&] {
        check({{{"a", std::string(300, 'x')}}, {{Kind::patch, "a", std::string(300, 'y')}}, {}},
              ok);
      },
      "event limit");
  rejects(
      [&] {
        check({{}, {{Kind::ack, "", ""}, {Kind::write, "missing", "x"}}, {}},
              [](const Snapshot &, const Context &) -> std::optional<std::string> {
                return "early failure";
              });
      },
      "validate whole program first");
  rejects([&] { check(s, {}); }, "empty invariant");
  Scenario many;
  for (int i = 0; i < 64; ++i)
    many.initial.emplace("f" + std::to_string(i), "x");
  many.run = {{Kind::create, "extra", ""}, {Kind::sync_dir, "", ""}};
  require(check(many, ok).status == Status::pass,
          "recovered snapshots may exceed initial file bound");
  many.initial.emplace("too_many", "x");
  rejects([&] { check(many, ok); }, "initial file limit");
  Scenario recovery_limit{
      {{"a", std::string(300, 'x')}}, {}, {{Kind::patch, "a", std::string(300, 'y')}}};
  rejects([&] { check(recovery_limit, ok); }, "oversized recovery is invalid input not violation");
  Scenario partial{{{"a", "old"}}, {}, {{Kind::write, "a", "new"}, {Kind::remove, "missing", ""}}};
  const auto partial_result = check(partial, ok);
  require(partial_result.status == Status::fail &&
              partial_result.witness->recovered.at("a") == "new",
          "recovery error preserves partial snapshot");
  rejects([&] { check({{}, {{Kind::remove_if_exists, "missing", ""}}, {}}, ok); },
          "conditional operation in run rejected by library");
  Scenario reuse{{{"a", "A"}, {"b", "B"}},
                 {{Kind::rename, "a", "b"},
                  {Kind::create, "a", ""},
                  {Kind::write, "a", "new-a"},
                  {Kind::write, "b", "new-b"},
                  {Kind::sync_file, "a", ""},
                  {Kind::sync_file, "b", ""}},
                 {}};
  require(replay(reuse, 6, {2, 3}, ok).crashed == Snapshot{{"a", "new-b"}, {"b", "B"}},
          "overwrite and recreate retain separate inode identities");
  require(replay(reuse, 6, {0, 1, 2, 3}, ok).crashed == Snapshot{{"a", "new-a"}, {"b", "new-b"}},
          "persisted namespace selects recreated inode");
  rejects([&] { replay(reuse, 6, {1, 2, 3}, ok); }, "recreate depends on prior rename");
  auto future =
      std::async(std::launch::async, [] { return check(replace(true), replacement).outcomes; });
  require(future.get() == check(replace(true), replacement).outcomes,
          "independent concurrent explorers deterministic");
  require(status_name(Status::pass) == "pass" && status_name(Status::incomplete) == "incomplete",
          "status names stable");
  if (failures != 0)
    return 1;
  std::cout << "core tests passed (crash model, recovery, replay, limits, concurrency)\n";
}
