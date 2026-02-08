//
// Created by Ilya Nyrkov on 19.08.25.
//

#ifndef TABLE_H
#define TABLE_H

#include "Row.h"
#include "Schema.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace memoria {

static bool valueTypeMatches(ColumnType t, const RowValue& v);

class Table {
  public:
    explicit Table(Schema schema) : schema_(std::move(schema)) {}

    // metadata
    [[nodiscard]] const Schema& getSchema() const noexcept;
    [[nodiscard]] std::size_t rowCount() const noexcept;

    // mutations (validate arity & types against schema)
    void insertRow(Row row);
    void deleteAllRows();

    template <class Pred> std::size_t deleteWhere(Pred pred) {
        const auto old = rows_.size();
        rows_.erase(
            std::remove_if(rows_.begin(), rows_.end(), [&](const Row& r) { return pred(r); }),
            rows_.end());
        return old - rows_.size();
    }

    template <class Pred>
    std::size_t updateWhere(Pred pred,
                            const std::vector<std::pair<std::size_t, RowValue>>& assignments) {
        for (const auto& [idx, val] : assignments) {
            if (idx >= schema_.size())
                throw std::out_of_range("Assignment column index out of range");
            const auto& col = schema_.columns().at(idx);
            const bool ok =
                (col.type == ColumnType::Int && std::holds_alternative<int64_t>(val)) ||
                (col.type == ColumnType::Str && std::holds_alternative<std::string>(val));
            if (!ok)
                throw std::invalid_argument("Assignment type mismatch");
        }

        std::size_t count = 0;

        // ---- index-based iteration avoids the rvalue-binding issue
        for (std::size_t i = 0, n = rows_.size(); i < n; ++i) {
            Row& r = rows_.at(i); // mutable reference
            if (!pred(r))
                continue;
            ++count;

            for (const auto& [idx, val] : assignments) {
                if (const auto* pi = std::get_if<int64_t>(&val)) {
                    r.at(idx).emplace<int64_t>(*pi);
                } else {
                    const auto& ps = std::get<std::string>(val);
                    r.at(idx).emplace<std::string>(ps);
                }
            }
        }

        return count;
    }

    template <class Pred> [[nodiscard]] std::vector<Row> getRowsWhere(Pred pred) const {
        std::vector<Row> out;
        out.reserve(rows_.size());
        for (const auto& r : rows_) {
            if (pred(r))
                out.push_back(r);
        }
        return out;
    }

    template <class Pred>
    [[nodiscard]] std::vector<Row> getColumnRowsWhere(const std::vector<std::size_t>& columnIndices,
                                                      Pred pred) const {
        for (auto idx : columnIndices) {
            if (idx >= schema_.size())
                throw std::out_of_range("Projection index out of range");
        }

        std::vector<Row> out;
        for (const auto& r : rows_) {
            if (!pred(r))
                continue;
            std::vector<RowValue> projected;
            projected.reserve(columnIndices.size());
            for (auto idx : columnIndices) {
                projected.push_back(r.at(idx)); // copy cell
            }
            out.emplace_back(std::move(projected));
        }
        return out;
    }

  private:
    Schema schema_;
    std::vector<Row> rows_;
};

} // namespace memoria

#endif // TABLE_H
