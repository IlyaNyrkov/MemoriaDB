//
// Created by Ilya Nyrkov on 19.08.25.
//

#ifndef STATEMENT_H
#define STATEMENT_H

#include <string>
#include <optional>
#include "Row.h"
#include "Schema.h"

// ==== Where Statements ====

namespace memoria {
    enum class CompareOp { Eq, Neq, Lt, Gt, Le, Ge };

    struct Comparison {
        std::string column;
        CompareOp   op;
        RowValue literal;
    };

    struct And;
    struct Or;

    using Where = Comparison;
    // recursive definition,
    using WhereExpr = std::variant<Comparison, And, Or>;
    struct And { std::unique_ptr<WhereExpr> lhs, rhs; };
    struct Or  { std::unique_ptr<WhereExpr> lhs, rhs; };

    // ===== SQL Statements =====
    struct CreateTable {
        std::string tableName;
        Schema schema;
    };

    struct Insert {
        std::string tableName;
        std::vector<std::string> columnNames;
        std::vector<std::vector<RowValue>> rows;
    };

    struct Delete {
        std::string table;
        // if nullopt -> delete all rows
        std::optional<WhereExpr> where;
    };

    struct Assignment {
        std::string column;
        RowValue    value;
    };

    struct Update {
        std::string table;
        // SET col = value, ...
        std::vector<Assignment> set;
        std::optional<WhereExpr> where;
    };

    struct Select {
        std::string table;
        // projection: either * or a list of names
        struct Star { };
        using Projection = std::variant<Star, std::vector<std::string>>;
        Projection projection;
        std::optional<WhereExpr> where;
    };

    using Statement = std::variant<CreateTable, Insert, Delete, Update, Select>;

}

#endif //STATEMENT_H
