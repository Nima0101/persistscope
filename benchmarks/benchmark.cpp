#include <persistscope/persistscope.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
  namespace ps = persistscope;
  constexpr std::size_t samples = 7;
  const ps::Invariant invariant = [](const ps::Snapshot &, const ps::Context &) {
    return std::nullopt;
  };
  std::cout << "shape,events,samples,nodes,outcomes,status,median_ms,min_ms,max_ms\n";
  for (const bool independent : {false, true}) {
    for (const std::size_t count : {8U, 12U, 16U}) {
      ps::Scenario scenario;
      for (std::size_t i = 0; i < count; ++i) {
        const auto name = independent ? "f" + std::to_string(i) : "f";
        scenario.initial[name] = "old";
        scenario.run.push_back({ps::Kind::write, name, std::to_string(i)});
      }
      std::vector<double> milliseconds;
      ps::Result result;
      for (std::size_t sample = 0; sample < samples; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        result = ps::check(scenario, invariant, {1000000, {}});
        const auto end = std::chrono::steady_clock::now();
        milliseconds.push_back(std::chrono::duration<double, std::milli>(end - start).count());
      }
      std::sort(milliseconds.begin(), milliseconds.end());
      std::cout << (independent ? "independent" : "chain") << ',' << count << ',' << samples << ','
                << result.nodes << ',' << result.outcomes << ',' << ps::status_name(result.status)
                << ',' << std::fixed << std::setprecision(3) << milliseconds[samples / 2] << ','
                << milliseconds.front() << ',' << milliseconds.back() << '\n';
      if (result.status != ps::Status::pass)
        return 1;
    }
  }
}
