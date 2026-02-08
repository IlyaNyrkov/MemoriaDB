//
// Created by Ilya Nyrkov on 19.08.25.
//

#ifndef SCHEMA_H
#define SCHEMA_H

#include "Row.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace memoria {

enum class ColumnType { Int, Str };

struct Column {
    std::string name;
    ColumnType type;
};

class Schema {
  public:
    explicit Schema(std::vector<Column> columns);

    // basic metadata
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<Column>& columns() const noexcept;

    // name lookup
    [[nodiscard]] std::optional<std::size_t> index_of(std::string name) const noexcept;
    [[nodiscard]] std::size_t require_index(std::string name) const;

    [[nodiscard]] bool columnsPresent(std::vector<std::string>) const;

    // default value for column i: 0 for int, "" for str
    [[nodiscard]] RowValue default_value(std::size_t i) const;

  private:
    std::vector<Column> columns_;
    std::unordered_map<std::string, std::size_t> name_to_index_;
};

} // namespace memoria

#endif // SCHEMA_H
