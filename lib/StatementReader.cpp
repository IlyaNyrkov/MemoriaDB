// memoria/StatementReader.cpp
#include "memoria/StatementReader.h"

#include <cctype>
#include <string>
#include <utility>
#include <vector>
#include <ostream>
#include <istream>

namespace memoria {

// ---------- ctor / prompt ----------

StatementReader::StatementReader(std::istream& in)
    : in_(in),
      interactive_(&in == &std::cin),
      buffer_() {}

void StatementReader::printPrompt(std::ostream& out) const {
    if (interactive_) { out << "memoriadb> " << std::flush; }
}

// ---------- tiny helpers ----------

void StatementReader::trim(std::string& s) {
    auto issp = [](unsigned char c){ return std::isspace(c) != 0; };
    std::size_t i = 0;
    while (i < s.size() && issp(static_cast<unsigned char>(s[i]))) ++i;
    std::size_t j = s.size();
    while (j > i && issp(static_cast<unsigned char>(s[j-1]))) --j;
    if (i == 0 && j == s.size()) return;
    s.assign(s.begin()+static_cast<std::ptrdiff_t>(i), s.begin()+static_cast<std::ptrdiff_t>(j));
}

void StatementReader::stripComments(std::string& s) {
    std::string out;
    out.reserve(s.size());

    bool inBlock = false;
    bool inLine  = false;
    char quote   = 0;

    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (inLine) {
            if (c == '\n') { inLine = false; out.push_back(c); }
            continue;
        }
        if (inBlock) {
            if (c == '*' && i+1 < s.size() && s[i+1] == '/') { inBlock = false; ++i; }
            continue;
        }

        if (quote) {
            out.push_back(c);
            if (c == quote) quote = 0;
            continue;
        }

        // entering quotes?
        if (c == '\'' || c == '"') {
            quote = c;
            out.push_back(c);
            continue;
        }

        // start of comments?
        if (c == '-' && i+1 < s.size() && s[i+1] == '-') {
            inLine = true; ++i;
            continue;
        }
        if (c == '/' && i+1 < s.size() && s[i+1] == '*') {
            inBlock = true; ++i;
            continue;
        }

        out.push_back(c);
    }

    s.swap(out);
}

// Split by ';' outside quotes (comments should be stripped first).
std::vector<std::string>
StatementReader::splitBySemisOutsideQuotes(std::string_view sv) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    char quote = 0;

    for (std::size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == ';') {
            parts.emplace_back(std::string{sv.substr(start, i - start)});
            start = i + 1;
        }
    }
    if (start < sv.size())
        parts.emplace_back(std::string{sv.substr(start)});

    return parts;
}

// Extract the next complete statement from buffer_ (without trailing ';').
// Returns empty optional if no complete statement is present yet.


// ---------- main API ----------
std::optional<std::string> StatementReader::extractOneFromBuffer() {
    bool inBlock = false, inLine = false; char quote = 0;

    for (std::size_t i = 0; i < buffer_.size(); ++i) {
        char c = buffer_[i];

        if (inLine)  { if (c == '\n') inLine = false; continue; }
        if (inBlock) { if (c == '*' && i+1 < buffer_.size() && buffer_[i+1] == '/') { inBlock = false; ++i; } continue; }
        if (quote)   { if (c == quote) quote = 0; continue; }

        if (c == '\'' || c == '"') { quote = c; continue; }
        if (c == '-' && i+1 < buffer_.size() && buffer_[i+1] == '-') { inLine = true; ++i; continue; }
        if (c == '/' && i+1 < buffer_.size() && buffer_[i+1] == '*') { inBlock = true; ++i; continue; }

        if (c == ';') {
            std::string stmt = buffer_.substr(0, i);
            buffer_.erase(0, i + 1);

            stripComments(stmt);
            trim(stmt);
            if (stmt.empty()) {
                // skip empty statements caused by ;; or comment-only chunks
                return extractOneFromBuffer();
            }
            return stmt;
        }
    }
    return std::nullopt;
}

std::optional<std::string> StatementReader::next() {
    // try current buffer first
    if (auto s = extractOneFromBuffer()) return s;

    std::string line;
    while (std::getline(in_, line)) {
        buffer_.append(line);
        buffer_.push_back('\n');
        if (auto s = extractOneFromBuffer()) return s;
    }

    // EOF: return trailing non-empty (no trailing ';' required)
    stripComments(buffer_);
    trim(buffer_);
    if (buffer_.empty()) return std::nullopt;

    std::string last = std::move(buffer_);
    buffer_.clear();
    return last;
}

} // namespace memoria
