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

template <char mark, typename T, typename CharT>
void perform_log(
  std::basic_ostream<CharT> &out,
  std::basic_ostream<CharT> &(*color)(std::basic_ostream<CharT> &),
  const T &t
) {
  out << color << "[" << mark << "]: At " << t.loc << ": " << t.message << termcolor::reset << "\n";
}

error_queue &error_queue::operator<<(const error &e) {
  perform_log<'E'>(std::cerr, termcolor::red, e);
  return *this;
}

error_queue &error_queue::operator<<(const warning &w) {
  perform_log<'W'>(std::cerr, termcolor::yellow, w);
  return *this;
}

error_queue &error_queue::operator<<(const info &i) {
  perform_log<'I'>(std::cout, termcolor::blue, i);
  return *this;
}
