//
// Created by Ilya Nyrkov on 19.08.25.
//

#include "memoria/Database.h"

#include <iterator>

namespace memoria {
void Database::createTable(std::string tableName, Schema schema) {
    auto [it, inserted] = tables_.emplace(std::move(tableName), Table{std::move(schema)});
    if (!inserted) {
        throw std::invalid_argument("Table already exists");
    }
}

Table& Database::getTable(std::string tableName) {
    auto it = tables_.find(tableName);
    if (it == tables_.end())
        throw std::out_of_range("No such table");

    return it->second;
}

const Table& Database::getTable(std::string tableName) const {
    auto it = tables_.find(tableName);
    if (it == tables_.end())
        throw std::out_of_range("No such table");
    return it->second;
}

bool Database::hasTable(std::string tableName) const noexcept {
    return tables_.contains(tableName);
}
} // namespace memoria
