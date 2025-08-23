//
// Created by Ilya Nyrkov on 19.08.25.
//

#ifndef DATABASE_H
#define DATABASE_H

#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include "Table.h"

namespace memoria {
    class Database {
    public:
        Database() = default;
        void createTable(std::string tableName, Schema schema); // throws on duplicate
        Table&       getTable(std::string tableName);
        [[nodiscard]] const Table& getTable(std::string tableName) const;
        [[nodiscard]] bool hasTable(std::string tableName) const noexcept;

    private:
        std::unordered_map<std::string, Table> tables_;
    };

} // namespace memoria

#endif //DATABASE_H
