//
// Created by jay on 9/2/24.
//

#ifndef ERROR_QUEUE_HPP
#define ERROR_QUEUE_HPP

#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "util.hpp"

namespace jayc {
struct location {
  std::string file;
  int line;
  int col;
};

inline std::ostream &operator<<(std::ostream &strm, const location &loc) {
  return strm << loc.file << " (" << loc.line << ":" << loc.col << ")";
}

struct info {
  location loc;
  std::string message;
};
struct warning {
  location loc;
  std::string message;
};
struct error {
  location loc;
  std::string message;
};

struct any_error_happened final : std::runtime_error {
  using std::runtime_error::runtime_error;

  // ReSharper disable once CppNonExplicitConvertingConstructor
  inline any_error_happened(const std::string &what) : std::runtime_error(what) {} // NOLINT(*-explicit-constructor)
};

class error_queue {
public:
  error_queue(const error_queue&) = delete;
  error_queue(error_queue&&) = delete;
  error_queue& operator=(const error_queue&) = delete;
  error_queue& operator=(error_queue&&) = delete;

  static error_queue& get();

  error_queue &operator<<(const info &i);
  error_queue &operator<<(const warning &w);
  error_queue &operator<<(const error &e);

  inline void mute_info() { muted[0] = true; }
  inline void mute_warning() { muted[1] = true; }
  inline void mute_error() { muted[2] = true; }
  inline void unmute_info() { muted[0] = false; }
  inline void unmute_warning() { muted[1] = false; }
  inline void unmute_error() { muted[2] = false; }

  inline void next_phase(const bool cleanup = false) {
    if(cleanup) queue.clear();
    queue.emplace_back();
  }

  [[nodiscard]] constexpr size_t phase_info() const {
    return jaydk::count_of<info>(queue.back());
  }

  [[nodiscard]] constexpr size_t phase_warning() const {
    return jaydk::count_of<warning>(queue.back());
  }

  [[nodiscard]] constexpr size_t phase_error() const {
    return jaydk::count_of<error>(queue.back());
  }

  inline void enable_throw_on_error() { throw_on_error = true; }
  inline void disable_throw_on_error() { throw_on_error = false; }

  constexpr ~error_queue() = default;
private:
  constexpr error_queue() : queue{std::vector<msg>{}} {}

  using msg = std::variant<info, warning, error>;
  bool muted[3] = { false, false, false };
  bool throw_on_error = false;
  std::vector<std::vector<msg>> queue;
};

inline static error_queue &logger = error_queue::get();
}

#endif //ERROR_QUEUE_HPP
