//
// Created by Ilya Nyrkov on 19.08.25.
//

#include "memoria/Parser.h"

#include <charconv>
#include <memory>

namespace memoria {
// -------------------- tiny utilities --------------------

static inline bool is_space(char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
        return true;
    default:
        return false;
    }
}

static inline void skipSpaces(std::string_view s, std::size_t& i) {
    while (i < s.size() && is_space(s[i]))
        ++i;
}

static std::string parseIdent(std::string_view s, std::size_t& i) {
    skipSpaces(s, i);
    if (i >= s.size() || !(std::isalpha(static_cast<unsigned char>(s[i])) || s[i] == '_'))
        throw ParseError("Expected identifier");
    std::size_t j = i + 1;
    while (j < s.size()) {
        char c = s[j];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            ++j;
            continue;
        }
        break;
    }
    std::string out(s.substr(i, j - i));
    i = j;
    return out;
}

static int64_t parseInt64(std::string_view s, std::size_t& i) {
    skipSpaces(s, i);
    std::size_t start = i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        ++i;
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i])))
        throw ParseError("Expected integer literal");
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
        ++i;
    int64_t v{};
    auto sv = s.substr(start, i - start);
    auto res = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    if (res.ec != std::errc{})
        throw ParseError("Invalid integer literal");
    return v;
}

static std::string parseQuoted(std::string_view s, std::size_t& i) {
    skipSpaces(s, i);
    if (i >= s.size() || (s[i] != '\'' && s[i] != '"'))
        throw ParseError("Expected quoted string");
    char quote = s[i++];
    std::string out;
    while (i < s.size()) {
        char c = s[i++];
        if (c == quote) {
            // simple SQL-style doubling not supported; assignment spec keeps it
            // simple
            return out;
        }
        out.push_back(c);
    }
    throw ParseError("Unterminated string literal");
}

static bool starts_with(std::string_view s, std::string_view kw) {
    return s.size() >= kw.size() && s.substr(0, kw.size()) == kw;
}

// -------------------- Parser private helpers --------------------
std::string Parser::trimLeft(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && is_space(s[i]))
        ++i;
    return std::string{s.substr(i)};
}

std::vector<std::string> Parser::splitOutsideQuotes(std::string_view script) {
    std::vector<std::string> parts;
    char quote = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < script.size(); ++i) {
        char c = script[i];
        if ((c == '\'' || c == '"')) {
            if (quote == 0)
                quote = c;
            else if (quote == c)
                quote = 0;
        } else if (c == ';' && quote == 0) {
            // push chunk without the ';'
            parts.emplace_back(std::string{script.substr(start, i - start)});
            start = i + 1;
        }
    }
    if (start < script.size()) {
        parts.emplace_back(std::string{script.substr(start)});
    }
    return parts;
}

std::string Parser::normalizeOne(std::string_view s) {
    // trim left
    std::size_t i = 0;
    while (i < s.size() && is_space(s[i]))
        ++i;
    // trim right (and optional trailing ;)
    std::size_t j = s.size();
    while (j > i && is_space(s[j - 1]))
        --j;
    if (j > i && s[j - 1] == ';') {
        --j;
        while (j > i && is_space(s[j - 1]))
            --j;
    }
    return std::string{s.substr(i, j - i)};
}

std::pair<std::string, std::optional<std::string>> Parser::peelWhere(std::string_view normalized) {
    // find WHERE outside quotes
    char quote = 0;
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        char c = normalized[i];
        if ((c == '\'' || c == '"')) {
            if (quote == 0)
                quote = c;
            else if (quote == c)
                quote = 0;
        } else if (quote == 0 && c == 'W') {
            constexpr std::string_view KW{"WHERE"};
            if (i + KW.size() <= normalized.size() && normalized.substr(i, KW.size()) == KW) {
                // Ensure there's whitespace before 'W' or it's not needed (start)
                if (i > 0 && !is_space(normalized[i - 1]))
                    continue;
                std::size_t tail = i + KW.size();
                // consume following spaces
                while (tail < normalized.size() && is_space(normalized[tail]))
                    ++tail;
                std::string base = std::string{normalized.substr(0, i)};
                std::string where = std::string{normalized.substr(tail)};
                // trim base right
                std::size_t bj = base.size();
                while (bj > 0 && is_space(base[bj - 1]))
                    --bj;
                base.resize(bj);
                return {std::move(base), where.empty()
                                             ? std::optional<std::string>{}
                                             : std::optional<std::string>{std::move(where)}};
            }
        }
    }
    // no WHERE
    return {std::string{normalized}, std::nullopt};
}

// -------------------- WHERE parsing (recursive descent) --------------------

static CompareOp parseOp(std::string_view s, std::size_t& i) {
    skipSpaces(s, i);
    if (i + 2 <= s.size()) {
        if (s[i] == '!' && s[i + 1] == '=') {
            i += 2;
            return CompareOp::Neq;
        }
        if (s[i] == '<' && s[i + 1] == '=') {
            i += 2;
            return CompareOp::Le;
        }
        if (s[i] == '>' && s[i + 1] == '=') {
            i += 2;
            return CompareOp::Ge;
        }
    }
    if (i < s.size()) {
        if (s[i] == '=') {
            ++i;
            return CompareOp::Eq;
        }
        if (s[i] == '<') {
            ++i;
            return CompareOp::Lt;
        }
        if (s[i] == '>') {
            ++i;
            return CompareOp::Gt;
        }
    }
    throw ParseError("Expected comparison operator");
}

static RowValue parseLiteral(std::string_view s, std::size_t& i) {
    skipSpaces(s, i);
    if (i >= s.size())
        throw ParseError("Expected literal");
    if (s[i] == '\'' || s[i] == '"') {
        return RowValue{parseQuoted(s, i)};
    } else {
        return RowValue{parseInt64(s, i)};
    }
}

static WhereExpr parseWherePrimary(std::string_view s, std::size_t& i) {
    skipSpaces(s, i);
    if (i < s.size() && s[i] == '(') {
        ++i;
        WhereExpr inner;

        // OR-level
        auto parseOr = [&](auto&& self) -> WhereExpr {
            // AND-level
            auto parseAnd = [&](auto&& self2) -> WhereExpr {
                WhereExpr left = parseWherePrimary(s, i);
                while (true) {
                    std::size_t save = i;
                    skipSpaces(s, i);
                    if (i + 3 <= s.size() && s.substr(i, 3) == "AND") {
                        // ensure delimiter
                        if (i + 3 < s.size() &&
                            std::isalnum(static_cast<unsigned char>(s[i + 3]))) {
                            i = save;
                            break;
                        }
                        i += 3;
                        WhereExpr right = parseWherePrimary(s, i);
                        And a;
                        a.lhs = std::make_unique<WhereExpr>(std::move(left));
                        a.rhs = std::make_unique<WhereExpr>(std::move(right));
                        left = WhereExpr{std::move(a)};
                        continue;
                    }
                    break;
                }
                return left;
            };

            WhereExpr left = parseAnd(parseAnd);
            while (true) {
                std::size_t save = i;
                skipSpaces(s, i);
                if (i + 2 <= s.size() && s.substr(i, 2) == "OR") {
                    if (i + 2 < s.size() && std::isalnum(static_cast<unsigned char>(s[i + 2]))) {
                        i = save;
                        break;
                    }
                    i += 2;
                    WhereExpr right = parseAnd(parseAnd);
                    Or o;
                    o.lhs = std::make_unique<WhereExpr>(std::move(left));
                    o.rhs = std::make_unique<WhereExpr>(std::move(right));
                    left = WhereExpr{std::move(o)};
                    continue;
                }
                break;
            }
            return left;
        };

        inner = parseOr(parseOr);
        skipSpaces(s, i);
        if (i >= s.size() || s[i] != ')')
            throw ParseError("Expected ')'");
        ++i;
        return inner;
    }

    // comparison: <ident> <op> <literal>
    std::string col = parseIdent(s, i);
    CompareOp op = parseOp(s, i);
    RowValue lit = parseLiteral(s, i);
    return WhereExpr{Comparison{std::move(col), op, std::move(lit)}};
}

WhereExpr Parser::parseWhere(std::string_view whereTail) {
    std::size_t i = 0;

    // OR-level
    auto parseOr = [&](auto&& self) -> WhereExpr {
        // AND-level
        auto parseAnd = [&](auto&& self2) -> WhereExpr {
            WhereExpr left = parseWherePrimary(whereTail, i);
            while (true) {
                std::size_t save = i;
                skipSpaces(whereTail, i);
                if (i + 3 <= whereTail.size() && whereTail.substr(i, 3) == "AND") {
                    if (i + 3 < whereTail.size() &&
                        std::isalnum(static_cast<unsigned char>(whereTail[i + 3]))) {
                        i = save;
                        break;
                    }
                    i += 3;
                    WhereExpr right = parseWherePrimary(whereTail, i);
                    And a;
                    a.lhs = std::make_unique<WhereExpr>(std::move(left));
                    a.rhs = std::make_unique<WhereExpr>(std::move(right));
                    left = WhereExpr{std::move(a)};
                    continue;
                }
                break;
            }
            return left;
        };

        WhereExpr left = parseAnd(parseAnd);
        while (true) {
            std::size_t save = i;
            skipSpaces(whereTail, i);
            if (i + 2 <= whereTail.size() && whereTail.substr(i, 2) == "OR") {
                if (i + 2 < whereTail.size() &&
                    std::isalnum(static_cast<unsigned char>(whereTail[i + 2]))) {
                    i = save;
                    break;
                }
                i += 2;
                WhereExpr right = parseAnd(parseAnd);
                Or o;
                o.lhs = std::make_unique<WhereExpr>(std::move(left));
                o.rhs = std::make_unique<WhereExpr>(std::move(right));
                left = WhereExpr{std::move(o)};
                continue;
            }
            break;
        }
        return left;
    };

    WhereExpr root = parseOr(parseOr);
    skipSpaces(whereTail, i);
    if (i != whereTail.size())
        throw ParseError("Trailing tokens after WHERE");
    return root;
}

// -------------------- Base (non-WHERE) parsing --------------------
Statement parseCreateTableStmt(std::string_view s, std::size_t& i) {
    i += std::string_view("CREATE TABLE ").size();
    std::string table = parseIdent(s, i);
    skipSpaces(s, i);
    if (i >= s.size() || s[i] != '(')
        throw ParseError("Expected '(' after CREATE TABLE name");
    ++i;

    std::vector<Column> cols;
    while (true) {
        skipSpaces(s, i);
        if (i < s.size() && s[i] == ')') {
            ++i;
            break;
        }

        std::string cname = parseIdent(s, i);
        skipSpaces(s, i);

        if (i + 3 <= s.size() && s.substr(i, 3) == "int") {
            i += 3;
            cols.push_back(Column{std::move(cname), ColumnType::Int});
        } else if (i + 3 <= s.size() && s.substr(i, 3) == "str") {
            i += 3;
            cols.push_back(Column{std::move(cname), ColumnType::Str});
        } else {
            throw ParseError("Expected column type int or str");
        }

        skipSpaces(s, i);
        if (i < s.size() && s[i] == ',') {
            ++i;
            continue;
        }
        skipSpaces(s, i);
        if (i < s.size() && s[i] == ')') {
            ++i;
            break;
        }
    }

    skipSpaces(s, i);
    if (i != s.size())
        throw ParseError("Trailing tokens after CREATE TABLE");

    Schema schema{std::move(cols)};
    return Statement{CreateTable{std::move(table), std::move(schema)}};
}

static Statement parseInsertStmt(std::string_view s, std::size_t& i) {
    i += std::string_view("INSERT INTO ").size();

    std::string table = memoria::parseIdent(s, i);
    memoria::skipSpaces(s, i);

    // Optional column list
    std::vector<std::string> columnNames;
    if (i < s.size() && s[i] == '(') {
        ++i;
        while (true) {
            columnNames.push_back(memoria::parseIdent(s, i));
            memoria::skipSpaces(s, i);
            if (i < s.size() && s[i] == ',') {
                ++i;
                continue;
            }
            if (i < s.size() && s[i] == ')') {
                ++i;
                break;
            }
            throw ParseError("Expected ',' or ')' in column list");
        }
        memoria::skipSpaces(s, i);
    }

    if (!memoria::starts_with(s.substr(i), "VALUES"))
        throw ParseError("Expected VALUES");
    i += std::string_view("VALUES").size();

    // One or more parenthesized rows
    std::vector<std::vector<RowValue>> rows;
    while (true) {
        memoria::skipSpaces(s, i);
        if (i >= s.size() || s[i] != '(')
            throw ParseError("Expected '(' to start VALUES row");
        ++i;

        std::vector<RowValue> one;
        while (true) {
            RowValue v = memoria::parseLiteral(s, i);
            one.push_back(std::move(v));
            memoria::skipSpaces(s, i);
            if (i < s.size() && s[i] == ',') {
                ++i;
                continue;
            }
            if (i < s.size() && s[i] == ')') {
                ++i;
                break;
            }
            throw ParseError("Expected ',' or ')' in VALUES row");
        }
        rows.emplace_back(std::move(one));

        memoria::skipSpaces(s, i);
        if (i < s.size() && s[i] == ',') {
            ++i;
            continue;
        } // next row
        break; // no more rows
    }

    memoria::skipSpaces(s, i);
    if (i != s.size())
        throw ParseError("Trailing tokens after INSERT");

    // Insert{ tableName, columnNames, rows }
    return Statement{Insert{std::move(table), std::move(columnNames), std::move(rows)}};
}

// DELETE FROM <name>
static Statement parseDeleteStmt(std::string_view s, std::size_t& i) {
    i += std::string_view("DELETE FROM ").size();
    std::string table = parseIdent(s, i);
    skipSpaces(s, i);
    if (i != s.size())
        throw ParseError("Trailing tokens after DELETE FROM");
    return Statement{Delete{std::move(table)}};
}

// UPDATE <name> SET c = lit, c = lit, ...
static Statement parseUpdateStmt(std::string_view s, std::size_t& i) {
    i += std::string_view("UPDATE ").size();
    std::string table = parseIdent(s, i);
    skipSpaces(s, i);
    if (!starts_with(s.substr(i), "SET"))
        throw ParseError("Expected SET");
    i += std::string_view("SET").size();

    std::vector<Assignment> assigns;
    while (true) {
        std::string cname = parseIdent(s, i);
        skipSpaces(s, i);
        if (i >= s.size() || s[i] != '=')
            throw ParseError("Expected '=' in assignment");
        ++i;
        RowValue v = parseLiteral(s, i);
        assigns.push_back(Assignment{std::move(cname), std::move(v)});
        skipSpaces(s, i);
        if (i < s.size() && s[i] == ',') {
            ++i;
            continue;
        }
        break;
    }
    skipSpaces(s, i);
    if (i != s.size())
        throw ParseError("Trailing tokens after UPDATE");

    return Statement{Update{std::move(table), std::move(assigns)}};
}

static Statement parseSelectStmt(std::string_view s, std::size_t& i) {
    i += std::string_view("SELECT ").size();

    // Build the projection
    Select::Projection proj; // alias: std::variant<Select::Star, std::vector<std::string>>
    skipSpaces(s, i);
    if (i < s.size() && s[i] == '*') {
        ++i;
        proj = Select::Star{};
    } else {
        std::vector<std::string> cols;
        while (true) {
            cols.push_back(parseIdent(s, i));
            skipSpaces(s, i);
            if (i < s.size() && s[i] == ',') {
                ++i;
                continue;
            }
            break;
        }
        proj = std::move(cols);
    }

    // FROM <table>
    skipSpaces(s, i);
    if (!starts_with(s.substr(i), "FROM "))
        throw ParseError("Expected FROM");
    i += std::string_view("FROM ").size();
    std::string table = parseIdent(s, i);

    // no trailing tokens in the base
    skipSpaces(s, i);
    if (i != s.size())
        throw ParseError("Trailing tokens after SELECT");

    // NOTE: WHERE is handled upstream (peelWhere/parseWhere), so set where =
    // nullopt here.
    return Statement{Select{std::move(table), std::move(proj), std::nullopt}};
}

Statement Parser::parseBase(std::string_view base) {
    std::size_t i = 0;
    skipSpaces(base, i);

    if (starts_with(base.substr(i), "CREATE TABLE "))
        return parseCreateTableStmt(base, i);
    if (starts_with(base.substr(i), "INSERT INTO "))
        return parseInsertStmt(base, i);
    if (starts_with(base.substr(i), "DELETE FROM "))
        return parseDeleteStmt(base, i);
    if (starts_with(base.substr(i), "UPDATE "))
        return parseUpdateStmt(base, i);
    if (starts_with(base.substr(i), "SELECT "))
        return parseSelectStmt(base, i);

    throw ParseError("Unknown statement (keywords are case-sensitive)");
}

// -------------------- Public API --------------------
Statement Parser::prepareStatement(std::string_view sql) const {
    const std::string norm = normalizeOne(sql);
    if (norm.empty())
        throw ParseError("Empty statement");

    auto [base, whereTxt] = peelWhere(norm);
    Statement st = parseBase(base);

    if (!whereTxt) {
        // No WHERE present; we’re done.
        return st;
    }

    // We have a WHERE tail -> only valid for SELECT / UPDATE / DELETE
    WhereExpr w = parseWhere(*whereTxt);

    if (auto* sel = std::get_if<Select>(&st)) {
        sel->where.emplace(std::move(w));
        return st;
    }
    if (auto* upd = std::get_if<Update>(&st)) {
        upd->where.emplace(std::move(w));
        return st;
    }
    if (auto* del = std::get_if<Delete>(&st)) {
        del->where.emplace(std::move(w));
        return st;
    }

    // CREATE/INSERT must not have WHERE
    throw ParseError("WHERE is not allowed for this statement type");
}

std::vector<Statement> Parser::prepareStatements(std::string_view script) const {
    std::vector<Statement> out;
    for (auto& raw : splitOutsideQuotes(script)) {
        const std::string norm = normalizeOne(raw);
        if (norm.empty())
            continue;
        out.emplace_back(prepareStatement(norm));
    }
    return out;
}

bool Parser::ieqPrefix(std::string_view s, std::string_view kw) {
    // In this project keywords are *case-sensitive*. Use a strict prefix test.
    return starts_with(s, kw);
}

} // namespace memoria