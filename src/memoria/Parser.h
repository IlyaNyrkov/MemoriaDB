//
// Created by Ilya Nyrkov on 19.08.25.
//

#ifndef PARSER_H
#define PARSER_H

#include <memoria/Statement.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace memoria {
struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Parser {
  public:
    Parser() = default;

    // parse one sql statement
    [[nodiscard]] Statement prepareStatement(std::string_view sql) const;

    // parse multiple sql statements
    [[nodiscard]] std::vector<Statement> prepareStatements(std::string_view script) const;

  private:
    // ---------- low-level helpers (pure string munging) ----------
    [[nodiscard]] static std::string trimLeft(std::string_view s);

    [[nodiscard]] static std::vector<std::string> splitOutsideQuotes(std::string_view script);

    // remove surrounding spaces and trailing ;
    [[nodiscard]] static std::string normalizeOne(std::string_view s);

    // ---------- mid-level extraction ----------

    // Extract WHERE clause (if any) from the full normalized statement text
    // and return {base_without_where, where_text}.
    [[nodiscard]] static std::pair<std::string, std::optional<std::string>>
    peelWhere(std::string_view normalized);

    [[nodiscard]] static Statement parseBase(std::string_view base);

    [[nodiscard]] static WhereExpr parseWhere(std::string_view whereTail);

    // ---------- tiny utilities ----------
    [[nodiscard]] static bool ieqPrefix(std::string_view s,
                                        std::string_view kw); // case-sensitive keywords if you
                                                              // want: set to strict eq if needed
};

} // namespace memoria

#endif // PARSER_H
