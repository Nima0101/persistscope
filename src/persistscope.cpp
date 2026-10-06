#include <persistscope/persistscope.hpp>

#include <algorithm>
#include <utility>

namespace persistscope {
namespace {
constexpr std::size_t max_events = 256;
constexpr std::size_t max_value = 4096;
constexpr std::size_t max_operations = 128;
using Names = std::map<std::string, std::size_t>;

void validate_name(const std::string &name) {
  if (name.empty() || name.size() > 64 || name == "." || name == "..")
    throw Error("invalid virtual name");
  for (const char raw : name) {
    const auto c = static_cast<unsigned char>(raw);
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-' || c == '.'))
      throw Error("invalid virtual name");
  }
}
void validate_operation(const Operation &op) {
  if (op.value.size() > max_value)
    throw Error("content exceeds 4096 bytes");
  switch (op.kind) {
  case Kind::rename:
  case Kind::rename_if_exists:
    validate_name(op.value);
    [[fallthrough]];
  case Kind::create:
  case Kind::write:
  case Kind::patch:
  case Kind::remove:
  case Kind::sync_file:
  case Kind::remove_if_exists:
    validate_name(op.path);
    break;
  case Kind::sync_dir:
  case Kind::ack:
    if (!op.path.empty())
      throw Error("operation does not accept a path");
    break;
  default:
    throw Error("unknown operation kind");
  }
  if (op.kind != Kind::write && op.kind != Kind::patch && op.kind != Kind::rename &&
      op.kind != Kind::rename_if_exists && !op.value.empty())
    throw Error("operation does not accept a value");
  if (op.kind != Kind::patch && op.offset != 0)
    throw Error("operation does not accept an offset");
}
struct Event {
  bool data = false;
  std::size_t inode = 0;
  std::string content;
  std::string source;      // Empty for create; removed for rename/remove.
  std::string destination; // Empty for remove.
  std::vector<std::size_t> dependencies;
  bool forced = false;
};

class Machine {
public:
  explicit Machine(const Snapshot &initial) {
    for (const auto &[name, content] : initial) {
      validate_name(name);
      if (content.size() > max_value)
        throw Error("content exceeds 4096 bytes");
      names_.emplace(name, bytes_.size());
      bytes_.push_back(content);
    }
    initial_names_ = names_;
    initial_bytes_ = bytes_;
  }

  void apply(const Operation &op) {
    switch (op.kind) {
    case Kind::create: {
      if (names_.contains(op.path))
        throw Error("create requires an absent name");
      const auto inode = bytes_.size();
      Event event;
      event.inode = inode;
      event.destination = op.path;
      namespace_dependencies(event, op.path);
      add(std::move(event));
      bytes_.emplace_back();
      initial_bytes_.emplace_back();
      names_[op.path] = inode;
      break;
    }
    case Kind::write: {
      const auto inode = lookup(op.path);
      data_event(inode, op.value);
      break;
    }
    case Kind::patch: {
      const auto inode = lookup(op.path);
      const auto size = bytes_[inode].size();
      if (op.offset > size || op.value.size() > size - op.offset)
        throw Error("patch must fit within the existing file");
      if (op.value.size() > max_events - events.size())
        throw Error("mutation event limit exceeded");
      for (std::size_t i = 0; i < op.value.size(); ++i) {
        auto content = bytes_[inode];
        content[op.offset + i] = op.value[i];
        data_event(inode, content);
      }
      break;
    }
    case Kind::rename_if_exists:
      if (!names_.contains(op.path))
        break;
      [[fallthrough]];
    case Kind::rename: {
      const auto inode = lookup(op.path);
      if (op.path == op.value)
        break;
      Event event;
      event.inode = inode;
      event.source = op.path;
      event.destination = op.value;
      namespace_dependencies(event, op.path);
      namespace_dependencies(event, op.value);
      add(std::move(event));
      names_.erase(op.path);
      names_[op.value] = inode;
      break;
    }
    case Kind::remove_if_exists:
      if (!names_.contains(op.path))
        break;
      [[fallthrough]];
    case Kind::remove: {
      (void)lookup(op.path);
      Event event;
      event.source = op.path;
      namespace_dependencies(event, op.path);
      add(std::move(event));
      names_.erase(op.path);
      break;
    }
    case Kind::sync_file: {
      const auto inode = lookup(op.path);
      const auto last = last_data_.find(inode);
      if (last != last_data_.end())
        force(last->second);
      break;
    }
    case Kind::sync_dir:
      for (std::size_t i = 0; i < events.size(); ++i)
        if (!events[i].data)
          force(i);
      break;
    case Kind::ack:
      acknowledged = true;
      break;
    default:
      throw Error("unknown operation kind");
    }
  }

  Snapshot snapshot(const std::vector<bool> &selected) const {
    auto names = initial_names_;
    auto bytes = initial_bytes_;
    for (std::size_t i = 0; i < events.size(); ++i) {
      if (!selected[i])
        continue;
      const auto &event = events[i];
      if (event.data) {
        bytes[event.inode] = event.content;
      } else {
        if (!event.source.empty())
          names.erase(event.source);
        if (!event.destination.empty())
          names[event.destination] = event.inode;
      }
    }
    return materialize(names, bytes);
  }
  Snapshot live() const { return materialize(names_, bytes_); }
  std::vector<Event> events;
  bool acknowledged = false;

private:
  static Snapshot materialize(const Names &names, const std::vector<std::string> &bytes) {
    Snapshot result;
    for (const auto &[name, inode] : names)
      result.emplace(name, bytes[inode]);
    return result;
  }
  std::size_t lookup(const std::string &path) const {
    const auto it = names_.find(path);
    if (it == names_.end())
      throw Error("operation requires an existing name");
    return it->second;
  }
  void namespace_dependencies(Event &event, const std::string &path) const {
    const auto it = last_name_.find(path);
    if (it != last_name_.end() && std::find(event.dependencies.begin(), event.dependencies.end(),
                                            it->second) == event.dependencies.end())
      event.dependencies.push_back(it->second);
  }
  void add(Event event) {
    if (events.size() == max_events)
      throw Error("mutation event limit exceeded");
    const auto id = events.size();
    if (event.data)
      last_data_[event.inode] = id;
    else {
      if (!event.source.empty())
        last_name_[event.source] = id;
      if (!event.destination.empty())
        last_name_[event.destination] = id;
    }
    events.push_back(std::move(event));
  }
  void data_event(std::size_t inode, const std::string &content) {
    Event event;
    event.data = true;
    event.inode = inode;
    event.content = content;
    const auto last = last_data_.find(inode);
    if (last != last_data_.end())
      event.dependencies.push_back(last->second);
    add(std::move(event));
    bytes_[inode] = content;
  }
  void force(std::size_t id) {
    if (events[id].forced)
      return;
    events[id].forced = true;
    for (const auto dependency : events[id].dependencies)
      force(dependency);
  }
  Names names_;
  Names initial_names_;
  std::vector<std::string> bytes_;
  std::vector<std::string> initial_bytes_;
  std::map<std::string, std::size_t> last_name_;
  std::map<std::size_t, std::size_t> last_data_;
};

void validate(const Scenario &scenario, const Invariant &invariant) {
  if (!invariant)
    throw Error("invariant callback is required");
  if (scenario.initial.size() > 64)
    throw Error("initial snapshot exceeds 64 files");
  if (scenario.run.size() > max_operations || scenario.recovery.size() > max_operations)
    throw Error("program exceeds 128 operations");
  for (const auto &op : scenario.run) {
    validate_operation(op);
    if (op.kind == Kind::rename_if_exists || op.kind == Kind::remove_if_exists)
      throw Error("conditional operations are recovery-only");
  }
  std::size_t recovery_events = 0;
  for (const auto &op : scenario.recovery) {
    validate_operation(op);
    if (op.kind == Kind::patch)
      recovery_events += op.value.size();
    else if (op.kind != Kind::sync_file && op.kind != Kind::sync_dir && op.kind != Kind::ack)
      ++recovery_events;
    if (recovery_events > max_events)
      throw Error("recovery mutation event limit exceeded");
  }
  Machine machine(scenario.initial);
  for (const auto &op : scenario.run)
    machine.apply(op);
}
Witness evaluate(const Scenario &scenario, const Machine &machine, std::size_t cut,
                 const std::vector<bool> &selected, const Invariant &invariant) {
  Witness witness;
  witness.cut = cut;
  witness.acknowledged = machine.acknowledged;
  for (std::size_t i = 0; i < selected.size(); ++i)
    if (selected[i])
      witness.durable_events.push_back(i);
  witness.crashed = machine.snapshot(selected);
  witness.recovered = witness.crashed;
  Machine recovery(witness.crashed);
  try {
    for (const auto &op : scenario.recovery)
      recovery.apply(op);
    witness.recovered = recovery.live();
  } catch (const Error &error) {
    witness.recovered = recovery.live();
    witness.reason = std::string("recovery error: ") + error.what();
    return witness;
  }
  if (auto error = invariant(witness.recovered, {cut, machine.acknowledged})) {
    witness.reason = error->empty() ? "invariant violated" : *error;
  }
  return witness;
}

class Explorer {
public:
  Explorer(const Scenario &scenario, const Invariant &invariant, const Limits &limits)
      : scenario_(scenario), invariant_(invariant), limits_(limits) {}
  bool visit(const Machine &machine, std::size_t cut, std::size_t index,
             std::vector<bool> &selected) {
    if (result.nodes == limits_.max_nodes || (limits_.cancelled && limits_.cancelled()))
      return false;
    ++result.nodes;
    if (index == machine.events.size()) {
      ++result.outcomes;
      auto witness = evaluate(scenario_, machine, cut, selected, invariant_);
      if (!witness.reason.empty()) {
        result.status = Status::fail;
        result.witness = std::move(witness);
        return false;
      }
      return true;
    }
    const auto &event = machine.events[index];
    if (!event.forced) {
      selected[index] = false;
      if (!visit(machine, cut, index + 1, selected))
        return false;
    }
    const bool available = std::all_of(event.dependencies.begin(), event.dependencies.end(),
                                       [&](std::size_t id) { return selected[id]; });
    if (available) {
      selected[index] = true;
      if (!visit(machine, cut, index + 1, selected))
        return false;
    }
    selected[index] = false;
    return true;
  }
  Result result;

private:
  const Scenario &scenario_;
  const Invariant &invariant_;
  const Limits &limits_;
};
} // namespace

Result check(const Scenario &scenario, const Invariant &invariant, const Limits &limits) {
  if (limits.max_nodes == 0 || limits.max_nodes > 10000000)
    throw Error("max_nodes must be 1..10000000");
  validate(scenario, invariant);
  Machine machine(scenario.initial);
  Explorer explorer(scenario, invariant, limits);
  for (std::size_t cut = 0; cut <= scenario.run.size(); ++cut) {
    std::vector<bool> selected(machine.events.size(), false);
    if (!explorer.visit(machine, cut, 0, selected))
      return explorer.result;
    ++explorer.result.cuts_completed;
    if (cut < scenario.run.size())
      machine.apply(scenario.run[cut]);
  }
  explorer.result.status = Status::pass;
  return explorer.result;
}

Witness replay(const Scenario &scenario, std::size_t cut,
               const std::vector<std::size_t> &durable_events, const Invariant &invariant) {
  validate(scenario, invariant);
  if (cut > scenario.run.size())
    throw Error("cut exceeds program length");
  Machine machine(scenario.initial);
  for (std::size_t i = 0; i < cut; ++i)
    machine.apply(scenario.run[i]);
  std::vector<bool> selected(machine.events.size(), false);
  std::optional<std::size_t> previous;
  for (const auto id : durable_events) {
    if (id >= selected.size() || (previous && id <= *previous))
      throw Error("invalid durable event IDs");
    selected[id] = true;
    previous = id;
  }
  for (std::size_t i = 0; i < machine.events.size(); ++i) {
    const auto &event = machine.events[i];
    if (event.forced && !selected[i])
      throw Error("replay omits a forced event");
    if (selected[i]) {
      for (const auto dependency : event.dependencies)
        if (!selected[dependency])
          throw Error("replay is not dependency-closed");
    }
  }
  return evaluate(scenario, machine, cut, selected, invariant);
}
std::string_view status_name(Status status) {
  switch (status) {
  case Status::pass:
    return "pass";
  case Status::fail:
    return "fail";
  case Status::incomplete:
    return "incomplete";
  }
  throw Error("invalid result status");
}
} // namespace persistscope
