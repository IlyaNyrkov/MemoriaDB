//
// Created by Ilya Nyrkov on 20.08.25.
//

#ifndef STATEMENTEXECUTOR_H
#define STATEMENTEXECUTOR_H

#include "memoria/Database.h"
#include "memoria/Statement.h"
#include <memoria/Row.h>
#include <memory>
#include <optional>

namespace memoria {
struct QueryResult {
  std::vector<std::string> header;
  std::vector<Row> rows;
};

class StatementExecutor {
public:
  explicit StatementExecutor(Database &db) : db_(db) {}

  // High-level single entry point.
  // - CREATE/INSERT/UPDATE/DELETE: returns std::nullopt (side effects only)
  // - SELECT: returns QueryResult
  [[nodiscard]] std::optional<QueryResult> execute(const Statement &st);

  // Fine-grained operations (useful for tests or REPL routing)
  void execCreateTable(
      const CreateTable &st) const; // throws on duplicate table / bad schema
  void execInsert(const Insert &st) const; // throws on arity/type mismatch
  std::size_t execDelete(const Delete &st) const; // returns rows removed
  std::size_t execUpdate(const Update &st) const; // returns rows updated
  QueryResult execSelect(const Select &st) const; // returns projected rows

private:
  Database &db_;

  // ---- helpers (pure compilation/validation; no side effects) ----

  // Build a predicate the Table can consume. Type-erased for header simplicity.
  using Pred = std::function<bool(const Row &)>;
  [[nodiscard]] Pred compileWhere(const WhereExpr &expr,
                                  const Schema &schema) const;

  // Turn SELECT projection into column indices (empty => STAR/*)
  [[nodiscard]] std::vector<std::size_t>
  compileProjection(const Select::Projection &proj, const Schema &schema) const;

  // Map Update assignments (by name) to (column index, value) with type checks
  [[nodiscard]] std::vector<std::pair<std::size_t, RowValue>>
  compileAssignments(const std::vector<Assignment> &sets,
                     const Schema &schema) const;

  // Validate Insert column list vs schema and return indices order for each row
  // If Insert has no column list, the natural schema order [0..n) is used.
  [[nodiscard]] std::vector<std::size_t>
  compileInsertColumnOrder(const Insert &st, const Schema &schema) const;

  // Reorder/validate a VALUES row according to indices; throws on arity/type
  // mismatch
  [[nodiscard]] Row
  makeRowForInsert(const std::vector<RowValue> &values,
                   const std::vector<std::size_t> &columnOrder,
                   const Schema &schema) const;
};
} // namespace memoria

#endif // STATEMENTEXECUTOR_H
