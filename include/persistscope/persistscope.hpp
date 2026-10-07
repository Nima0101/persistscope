#ifndef PERSISTSCOPE_PERSISTSCOPE_HPP_INCLUDED
#define PERSISTSCOPE_PERSISTSCOPE_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace persistscope {
inline constexpr std::string_view version = "0.1.0";
inline constexpr std::string_view model = "ordered-file-v1";

class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

enum class Kind : std::uint8_t {
  create,
  write,
  patch,
  rename,
  remove,
  sync_file,
  sync_dir,
  ack,
  rename_if_exists,
  remove_if_exists
};
struct Operation {
  Kind kind;
  std::string path;
  std::string value; // Data for write/patch, destination for rename.
  std::size_t offset = 0;
};
using Snapshot = std::map<std::string, std::string>;
struct Scenario {
  Snapshot initial;
  std::vector<Operation> run;
  std::vector<Operation> recovery;
};
struct Context {
  std::size_t cut = 0; // Number of completed run operations.
  bool acknowledged = false;
};
// Return an explanation on violation; nullopt means the invariant holds.
using Invariant = std::function<std::optional<std::string>(const Snapshot &, const Context &)>;
struct Limits {
  std::size_t max_nodes = 100000;
  std::function<bool()> cancelled;
};
enum class Status : std::uint8_t { pass, fail, incomplete };
struct Witness {
  std::size_t cut = 0;
  std::vector<std::size_t> durable_events; // Zero-based IDs, ascending.
  Snapshot crashed;
  Snapshot recovered;
  bool acknowledged = false;
  std::string reason;
};
struct Result {
  Status status = Status::incomplete;
  std::size_t nodes = 0;
  std::size_t outcomes = 0;
  std::size_t cuts_completed = 0;
  std::optional<Witness> witness;
};
// All malformed scenarios throw Error, including invalid operations after an
// earlier potential counterexample. Callback exceptions propagate unchanged.
Result check(const Scenario &scenario, const Invariant &invariant, const Limits &limits = {});
Witness replay(const Scenario &scenario, std::size_t cut,
               const std::vector<std::size_t> &durable_events, const Invariant &invariant);
std::string_view status_name(Status status);
} // namespace persistscope

#endif // PERSISTSCOPE_PERSISTSCOPE_HPP_INCLUDED
