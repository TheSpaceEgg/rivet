#include "diag.hpp"
#include <iostream>
#include <string>

// ANSI colour codes (widely supported in VS Code + modern Windows)
static constexpr const char* RED    = "\033[31m";
static constexpr const char* YELLOW = "\033[33m";
static constexpr const char* BLUE   = "\033[34m";
static constexpr const char* RESET  = "\033[0m";

static const char* level_str(DiagLevel level) {
    switch (level) {
        case DiagLevel::Error:   return "error";
        case DiagLevel::Warning: return "warning";
        case DiagLevel::Note:    return "note";
    }
    return "error";
}

static const char* level_colour(DiagLevel level) {
    switch (level) {
        case DiagLevel::Error:   return RED;
        case DiagLevel::Warning: return YELLOW;
        case DiagLevel::Note:    return BLUE;
    }
    return RED;
}

bool DiagnosticEngine::report(DiagLevel level,
                              SourceLoc loc,
                              const std::string& message) const
{
    // Header: file:line:col <coloured level>: message
    std::cerr
        << src_.filename() << ":"
        << loc.line << ":"
        << loc.col << " "
        << level_colour(level)
        << level_str(level)
        << RESET
        << ": "
        << message
        << "\n";

    // Source line
    auto line = src_.line_text(loc.line);
    std::cerr << line << "\n";

    // Caret line: spaces up to (col-1), then ^
    int caret_pos = loc.col;
    if (caret_pos < 1) caret_pos = 1;

    for (int i = 1; i < caret_pos; ++i) std::cerr << ' ';
    std::cerr << "^\n";

    return false;
}
