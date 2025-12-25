#include "diag.hpp"
#include <iostream>
#include <string>

static const char* level_str(DiagLevel level) {
    switch (level) {
        case DiagLevel::Error:   return "error";
        case DiagLevel::Warning: return "warning";
        case DiagLevel::Note:    return "note";
    }
    return "error";
}

bool DiagnosticEngine::report(DiagLevel level, SourceLoc loc, const std::string& message) const {
    // Header: file:line:col level: message
    std::cerr << src_.filename() << ":" << loc.line << ":" << loc.col
              << " " << level_str(level) << ": " << message << "\n";

    // Source line
    auto line = src_.line_text(loc.line);
    std::cerr << line << "\n";

    // Caret line: spaces up to (col-1), then ^
    // (keep it simple: assumes ASCII; fine for now)
    int caret_pos = loc.col;
    if (caret_pos < 1) caret_pos = 1;

    for (int i = 1; i < caret_pos; ++i) std::cerr << ' ';
    std::cerr << "^\n";

    return false;
}
