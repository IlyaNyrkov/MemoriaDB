//
// Created by Ilya Nyrkov on 19.08.25.
//

#ifndef ROW_H
#define ROW_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace memoria {

using RowValue = std::variant<int64_t, std::string>;

class Row {
public:
  Row();
  explicit Row(std::vector<RowValue> data);
  Row(std::initializer_list<RowValue> init);

  [[nodiscard]] const RowValue &at(std::size_t i) const;
  [[nodiscard]] RowValue &at(std::size_t i);

  [[nodiscard]] std::size_t size() const noexcept;

private:
  std::vector<RowValue> data_;
};

} // namespace memoria

#endif // ROW_H
