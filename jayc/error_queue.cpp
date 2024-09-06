//
// Created by jay on 9/2/24.
//

#include <cstdint>
#include <iostream>
#include <termcolor/termcolor.hpp>

#include "error_queue.hpp"

using namespace jayc;

error_queue &error_queue::get() {
  static error_queue queue;
  return queue;
}

template <char mark, typename T, typename CharT, typename Contained>
void perform_log(
  const bool muted,
  std::basic_ostream<CharT> &out,
  std::basic_ostream<CharT> &(*color)(std::basic_ostream<CharT> &),
  const T &t,
  std::vector<Contained> &buffer
) {
  if(!muted) {
    out << color << "[" << mark << "]: At " << t.loc << ": " << t.message << termcolor::reset << "\n";
  }
  buffer.push_back(Contained{t});
}

error_queue &error_queue::operator<<(const error &e) {
  perform_log<'E'>(muted[2], std::cerr, termcolor::red, e, queue);

  if(throw_on_error) {
    throw any_error_happened(e.message);
  }

  return *this;
}

error_queue &error_queue::operator<<(const warning &w) {
  perform_log<'W'>(muted[1], std::cerr, termcolor::yellow, w, queue);
  return *this;
}

error_queue &error_queue::operator<<(const info &i) {
  perform_log<'I'>(muted[0], std::cout, termcolor::blue, i, queue);
  return *this;
}
