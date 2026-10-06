#include "persistscope/persistscope.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace ps = persistscope;

std::string quote(std::string_view value) {
  constexpr char hex[] = "0123456789abcdef";
  std::string out = "\"";
  for (const char raw : value) {
    const auto c = static_cast<unsigned char>(raw);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += raw;
    } else if (c < 32 || c >= 127) {
      out += "\\u00";
      out += hex[c >> 4];
      out += hex[c & 15];
    } else {
      out += raw;
    }
  }
  return out + '"';
}

std::size_t number(std::string_view value) {
  std::size_t result = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
    throw ps::Error("expected unsigned decimal integer");
  }
  return result;
}

void name(std::string_view value) {
  if (value.empty() || value.size() > 64 || value == "." || value == ".." ||
      !std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-' || c == '.';
      })) {
    throw ps::Error("invalid virtual file name");
  }
}
void validate_data(std::string_view value) {
  if (value.size() > 4096) {
    throw ps::Error("content exceeds 4096 bytes");
  }
}

std::vector<std::string> tokens(std::string_view line) {
  std::vector<std::string> result;
  std::size_t i = 0;
  while (i < line.size()) {
    if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r') {
      ++i;
      continue;
    }
    if (line[i] == '#') {
      break;
    }
    std::string token;
    if (line[i] == '"') {
      ++i;
      bool closed = false;
      while (i < line.size()) {
        char c = line[i++];
        if (static_cast<unsigned char>(c) < 32) {
          throw ps::Error("quoted tokens must contain printable ASCII");
        }
        if (c == '"') {
          closed = true;
          break;
        }
        if (c == '\\') {
          if (i == line.size() || (line[i] != '\\' && line[i] != '"')) {
            throw ps::Error("only quote and backslash escapes are supported");
          }
          c = line[i++];
        }
        token += c;
      }
      if (!closed) {
        throw ps::Error("unterminated quoted token");
      }
      if (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' &&
          line[i] != '#') {
        throw ps::Error("quoted tokens require a delimiter");
      }
    } else {
      while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' &&
             line[i] != '#') {
        if (line[i] == '"' || line[i] == '\\') {
          throw ps::Error("quote or backslash requires a quoted token");
        }
        token += line[i++];
      }
    }
    result.push_back(std::move(token));
  }
  return result;
}

void arity(const std::vector<std::string> &words, std::size_t count) {
  if (words.size() != count) {
    throw ps::Error("wrong number of arguments");
  }
}
struct Assertion {
  std::string kind;
  std::vector<std::string> args;
};
struct Parsed {
  ps::Scenario scenario;
  std::vector<Assertion> assertions;
};

Parsed parse(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw ps::Error("cannot open scenario file");
  }
  std::string text(65537, '\0');
  input.read(text.data(), static_cast<std::streamsize>(text.size()));
  text.resize(static_cast<std::size_t>(input.gcount()));
  if (input.bad()) {
    throw ps::Error("cannot read scenario file");
  }
  if (text.size() > 65536) {
    throw ps::Error("scenario exceeds 65536 bytes");
  }
  for (char raw : text) {
    const auto c = static_cast<unsigned char>(raw);
    if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c >= 127) {
      throw ps::Error("scenario must contain printable ASCII and whitespace only");
    }
  }
  Parsed parsed;
  enum class Section : std::uint8_t { header, initial, run, recover, check };
  Section section = Section::header;
  std::istringstream lines(text);
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(lines, line)) {
    ++line_number;
    try {
      const auto words = tokens(line);
      if (words.empty()) {
        continue;
      }
      const auto &op = words[0];
      if (section == Section::header) {
        if (words != std::vector<std::string>{"persistscope", "1"}) {
          throw ps::Error("expected persistscope 1 header");
        }
        section = Section::initial;
        continue;
      }
      if (op == "run" && section == Section::initial) {
        arity(words, 1);
        section = Section::run;
        continue;
      }
      if (op == "recover" && section == Section::run) {
        arity(words, 1);
        section = Section::recover;
        continue;
      }
      if (op == "check" && (section == Section::run || section == Section::recover)) {
        arity(words, 1);
        section = Section::check;
        continue;
      }
      if (section == Section::initial) {
        if (op != "initial") {
          throw ps::Error("expected initial or run");
        }
        arity(words, 3);
        name(words[1]);
        validate_data(words[2]);
        if (!parsed.scenario.initial.emplace(words[1], words[2]).second) {
          throw ps::Error("duplicate initial file");
        }
      } else if (section == Section::check) {
        if (op == "expect" || op == "expect_after_ack") {
          if (words.size() < 3) {
            throw ps::Error("expect requires a name and one or more values");
          }
          name(words[1]);
          for (std::size_t i = 2; i < words.size(); ++i) {
            validate_data(words[i]);
          }
        } else if (op == "exists") {
          arity(words, 2);
          name(words[1]);
        } else if (op == "equal") {
          arity(words, 3);
          name(words[1]);
          name(words[2]);
        } else {
          throw ps::Error("unknown assertion");
        }
        parsed.assertions.push_back({op, {words.begin() + 1, words.end()}});
      } else {
        ps::Operation operation{ps::Kind::ack, {}, {}, 0};
        if (op == "write" || op == "patch") {
          arity(words, op == "patch" ? 4 : 3);
          operation.kind = op == "write" ? ps::Kind::write : ps::Kind::patch;
          operation.path = words[1];
          operation.value = words.back();
          validate_data(operation.value);
          if (op == "patch") {
            operation.offset = number(words[2]);
          }
        } else if (op == "rename" || op == "rename_if_exists") {
          arity(words, 3);
          operation.kind = op == "rename" ? ps::Kind::rename : ps::Kind::rename_if_exists;
          operation.path = words[1];
          operation.value = words[2];
          name(operation.value);
        } else if (op == "create" || op == "remove" || op == "remove_if_exists" || op == "fsync") {
          arity(words, 2);
          operation.kind = op == "create"   ? ps::Kind::create
                           : op == "remove" ? ps::Kind::remove
                           : op == "fsync"  ? ps::Kind::sync_file
                                            : ps::Kind::remove_if_exists;
          operation.path = words[1];
        } else if (op == "sync_dir" || op == "ack") {
          arity(words, 1);
          operation.kind = op == "ack" ? ps::Kind::ack : ps::Kind::sync_dir;
        } else {
          throw ps::Error("unknown operation or misplaced section");
        }
        if (op != "ack" && op != "sync_dir") {
          name(operation.path);
        }
        if ((op == "rename_if_exists" || op == "remove_if_exists") && section != Section::recover) {
          throw ps::Error("conditional operations are recovery-only");
        }
        auto &program = section == Section::run ? parsed.scenario.run : parsed.scenario.recovery;
        program.push_back(std::move(operation));
      }
    } catch (const ps::Error &error) {
      throw ps::Error("line " + std::to_string(line_number) + ": " + error.what());
    }
  }
  if (section != Section::check) {
    throw ps::Error("scenario requires header, run and check sections");
  }
  return parsed;
}

ps::Invariant invariant(const Parsed &parsed) {
  return [&parsed](const ps::Snapshot &snapshot,
                   const ps::Context &context) -> std::optional<std::string> {
    for (const auto &assertion : parsed.assertions) {
      if (assertion.kind == "expect_after_ack" && !context.acknowledged) {
        continue;
      }
      const auto &args = assertion.args;
      const auto found = snapshot.find(args[0]);
      bool valid = found != snapshot.end();
      if (valid && assertion.kind == "equal") {
        const auto other = snapshot.find(args[1]);
        valid = other != snapshot.end() && found->second == other->second;
      } else if (valid && assertion.kind != "exists") {
        valid = std::find(args.begin() + 1, args.end(), found->second) != args.end();
      }
      if (!valid) {
        return assertion.kind + " failed for " + args[0];
      }
    }
    return std::nullopt;
  };
}

void snapshot_json(const ps::Snapshot &snapshot) {
  std::cout << '{';
  bool first = true;
  for (const auto &[key, value] : snapshot) {
    if (!first) {
      std::cout << ',';
    }
    first = false;
    std::cout << quote(key) << ':' << quote(value);
  }
  std::cout << '}';
}
void witness_json(const ps::Witness &witness) {
  std::cout << "{\"cut\":" << witness.cut << ",\"durable_events\":[";
  for (std::size_t i = 0; i < witness.durable_events.size(); ++i) {
    if (i != 0) {
      std::cout << ',';
    }
    std::cout << witness.durable_events[i];
  }
  std::cout << "],\"crashed\":";
  snapshot_json(witness.crashed);
  std::cout << ",\"recovered\":";
  snapshot_json(witness.recovered);
  std::cout << ",\"acknowledged\":" << (witness.acknowledged ? "true" : "false")
            << ",\"reason\":" << quote(witness.reason) << '}';
}
int output(const ps::Result &result, bool json) {
  const std::string_view status = result.status == ps::Status::pass   ? "PASS"
                                  : result.status == ps::Status::fail ? "FAIL"
                                                                      : "INCOMPLETE";
  if (json) {
    std::cout << "{\"schema_version\":1,\"model\":" << quote(ps::model)
              << ",\"status\":" << quote(status) << ",\"nodes\":" << result.nodes
              << ",\"outcomes\":" << result.outcomes
              << ",\"cuts_completed\":" << result.cuts_completed << ",\"witness\":";
    if (result.witness) {
      witness_json(*result.witness);
    } else {
      std::cout << "null";
    }
    std::cout << "}\n";
  } else {
    std::cout << status << " model=" << ps::model << " nodes=" << result.nodes
              << " outcomes=" << result.outcomes << " cuts_completed=" << result.cuts_completed
              << '\n';
    if (result.witness) {
      std::cout << "witness=";
      witness_json(*result.witness);
      std::cout << '\n';
    }
  }
  return result.status == ps::Status::pass ? 0 : result.status == ps::Status::fail ? 1 : 3;
}
} // namespace

int main(int argc, char **argv) {
  bool json = false;
  for (int i = 1; i < argc; ++i) {
    json = json || std::string_view(argv[i]) == "--json";
  }
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
      std::cout << "persistscope " << ps::version << '\n';
      return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
      std::cout << "persistscope check FILE [--max-nodes N] [--json]\n"
                   "persistscope replay FILE CUT EVENT_IDS [--json]\n";
      return 0;
    }
    if (argc < 3) {
      throw ps::Error("usage: persistscope check FILE [--max-nodes N] [--json]; replay FILE CUT "
                      "EVENT_IDS [--json]");
    }
    const std::string_view command(argv[1]);
    if (command != "check" && command != "replay") {
      throw ps::Error("unknown command");
    }
    if (command == "replay" && argc < 5) {
      throw ps::Error("replay requires FILE CUT EVENT_IDS");
    }
    ps::Limits limits;
    bool seen_json = false;
    bool seen_limit = false;
    for (int i = command == "check" ? 3 : 5; i < argc; ++i) {
      const std::string_view option(argv[i]);
      if (option == "--json" && !seen_json) {
        seen_json = true;
      } else if (option == "--max-nodes" && command == "check" && !seen_limit && i + 1 < argc) {
        seen_limit = true;
        limits.max_nodes = number(argv[++i]);
        if (limits.max_nodes == 0 || limits.max_nodes > 10000000) {
          throw ps::Error("max-nodes must be between 1 and 10000000");
        }
      } else {
        throw ps::Error("unknown, repeated or incomplete option");
      }
    }
    const auto parsed = parse(argv[2]);
    const auto test = invariant(parsed);
    if (command == "check") {
      return output(ps::check(parsed.scenario, test, limits), json);
    }
    const auto cut = number(argv[3]);
    std::vector<std::size_t> ids;
    std::string_view remaining(argv[4]);
    if (remaining != "-") {
      for (;;) {
        const auto comma = remaining.find(',');
        ids.push_back(number(remaining.substr(0, comma)));
        if (comma == std::string_view::npos) {
          break;
        }
        remaining.remove_prefix(comma + 1);
      }
    }
    auto witness = ps::replay(parsed.scenario, cut, ids, test);
    ps::Result result;
    result.status = witness.reason.empty() ? ps::Status::pass : ps::Status::fail;
    result.outcomes = 1;
    result.witness = std::move(witness);
    return output(result, json);
  } catch (const std::exception &error) {
    if (json) {
      std::cout << "{\"schema_version\":1,\"model\":" << quote(ps::model)
                << ",\"status\":\"INVALID\",\"error\":" << quote(error.what()) << "}\n";
    } else {
      std::cerr << "INVALID: " << quote(error.what()) << '\n';
    }
    return 2;
  }
}
