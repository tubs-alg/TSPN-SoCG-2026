//
// Created by Dominik Krupke on 10.01.23.
//

#ifndef TSPN_TIMER_H
#define TSPN_TIMER_H
#include <chrono>
namespace tspn::utils {
class Timer {

public:
  explicit Timer(double time_limit)
      : time_limit{time_limit},
        start{std::chrono::high_resolution_clock::now()} {}
  using TimePoint = decltype(std::chrono::high_resolution_clock::now());

  [[nodiscard]] double seconds() const {
    using namespace std::chrono;
    const auto now = high_resolution_clock::now();
    const auto time_used =
        static_cast<double>(duration_cast<milliseconds>(now - start).count()) /
        1000.0;
    return time_used;
  }

  [[nodiscard]] bool timeout() const { return seconds() >= time_limit; }

  double time_limit;
  TimePoint start;
};
} // namespace tspn::utils
#endif // TSPN_TIMER_H
