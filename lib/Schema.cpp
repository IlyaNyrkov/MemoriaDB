//
// Created by Ilya Nyrkov on 19.08.25.
//

#include "memoria/Schema.h"

namespace memoria {
Schema::Schema(std::vector<Column> columns) {
    name_to_index_.reserve(columns.size());
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto& name = columns.at(i).name;
        auto [it, inserted] = name_to_index_.emplace(name, i);
        if (!inserted) {
            throw std::invalid_argument("Duplicate column name: " + name);
        }
    }
    columns_ = std::move(columns);
}

std::size_t Schema::size() const noexcept {
    return columns_.size();
}

const std::vector<memoria::Column>& Schema::columns() const noexcept {
    return columns_;
}

std::optional<std::size_t> Schema::index_of(std::string name) const noexcept {
    auto it = name_to_index_.find(name);
    if (it == name_to_index_.end())
        return std::nullopt;
    return it->second;
}

std::size_t Schema::require_index(std::string name) const {
    if (auto idx = index_of(std::move(name)))
        return *idx;
    throw std::out_of_range("Column not found");
}

bool Schema::columnsPresent(std::vector<std::string> names) const {
    for (const auto& n : names) {
        if (!index_of(n).has_value())
            return false;
    }
    return true;
}

RowValue Schema::default_value(std::size_t i) const {
    const auto& col = columns_.at(i); // throws std::out_of_range if bad index
    switch (col.type) {
    case ColumnType::Int:
        return RowValue{std::in_place_type<int64_t>, 0};
    case ColumnType::Str:
        return RowValue{std::in_place_type<std::string>, ""};
    // Fallback (should be unreachable)
    default:
        throw std::invalid_argument("unknown column type");
    }
}
} // namespace memoria