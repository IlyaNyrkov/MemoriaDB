//
// Created by Ilya Nyrkov on 23.08.25.
//

#ifndef READER_H
#define READER_H

#include <iostream>

namespace memoria {
class StatementReader {
public:
  explicit StatementReader(std::istream &in);

  [[nodiscard]] bool readsFromCin() const noexcept { return &in_ == &std::cin; }

  // Returns the next statement without the trailing ';', or nullopt at EOF.
  std::optional<std::string> next();

  // Optional helper for REPL prompt
  void printPrompt(std::ostream &out = std::cout) const;

private:
  std::istream &in_;
  bool interactive_;
  std::string buffer_; // leftover after splitting

  static void stripComments(std::string &s); // -- and /* */
  static void trim(std::string &s);
  static std::vector<std::string> splitBySemisOutsideQuotes(std::string_view s);
  std::optional<std::string> extractOneFromBuffer();
};

} // namespace memoria

#endif // READER_H
