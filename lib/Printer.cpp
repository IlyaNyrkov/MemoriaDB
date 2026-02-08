//
// Created by Ilya Nyrkov on 20.08.25.
//

#include "memoria/Printer.h"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

namespace memoria {

static inline std::string repeat(char ch, std::size_t n) {
    return std::string(n, ch);
}

void Printer::printQueryResult(const QueryResult& qr) {
    printTable(qr.header, qr.rows);
    // Footer like psql: "(N rows)"
    out_ << "(" << qr.rows.size() << (qr.rows.size() == 1 ? " row)\n" : " rows)\n");
}

void Printer::printError(const std::exception& e) {
    err_ << "Error: " << e.what() << '\n';
}

void Printer::printHelpMessage(std::string_view prog) {
    out_ << prog
         << " — in-memory SQL (subset)\n"
            "Type SQL statements and terminate each with ';'.\n"
            "SELECT results are printed to stdout; errors go to stderr.\n"
            "Examples:\n"
            "  CREATE TABLE t (c1 STR, c2 INT);\n"
            "  INSERT INTO t VALUES ('a', 1), ('b', 2);\n"
            "  SELECT * FROM t WHERE c2 >= 2;\n"
            "Ctrl-D (Unix) / Ctrl-Z (Windows) to end input.\n";
}

void Printer::printAffected(std::size_t n) {
    out_ << "(" << n << " rows affected)\n";
}

std::string Printer::cellToString(const RowValue& v) {
    if (std::holds_alternative<int64_t>(v)) {
        return std::to_string(std::get<int64_t>(v));
    }
    return std::get<std::string>(v);
}

void Printer::printTable(const std::vector<std::string>& header, const std::vector<Row>& rows) {
    const std::size_t cols = header.size();
    if (cols == 0) {
        // Nothing to format—still emit a blank line for consistency.
        out_ << '\n';
        return;
    }

    // Compute column widths and numeric alignment flags
    std::vector<std::size_t> widths(cols, 0);
    std::vector<bool> isNumeric(cols, false);

    for (std::size_t i = 0; i < cols; ++i) {
        widths[i] = std::max<std::size_t>(widths[i], header[i].size());
    }

    for (const Row& r : rows) {
        for (std::size_t i = 0; i < cols; ++i) {
            const RowValue& cell = r.at(i);
            if (std::holds_alternative<int64_t>(cell))
                isNumeric[i] = true;
            const std::string s = cellToString(cell);
            widths[i] = std::max<std::size_t>(widths[i], s.size());
        }
    }

    // Print header
    for (std::size_t i = 0; i < cols; ++i) {
        if (i)
            out_ << " | ";
        out_ << std::left << std::setw(static_cast<int>(widths[i])) << header[i];
    }
    out_ << '\n';

    // Separator
    for (std::size_t i = 0; i < cols; ++i) {
        if (i)
            out_ << "-+-";
        out_ << repeat('-', widths[i]);
    }
    out_ << '\n';

    // Rows
    for (const Row& r : rows) {
        for (std::size_t i = 0; i < cols; ++i) {
            if (i)
                out_ << " | ";
            const std::string s = cellToString(r.at(i));
            if (isNumeric[i]) {
                out_ << std::right << std::setw(static_cast<int>(widths[i])) << s;
            } else {
                out_ << std::left << std::setw(static_cast<int>(widths[i])) << s;
            }
        }
        out_ << '\n';
    }
}

} // namespace memoria