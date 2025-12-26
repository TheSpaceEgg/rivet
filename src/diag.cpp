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
    if (level == DiagLevel::Error) {
        had_error_ = true;
    }

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

    // Source line extraction using the line number from loc
    // We use your existing src_.line_text(loc.line) method
    auto line_content = src_.line_text(loc.line);
    std::cerr << "  " << line_content << "\n";

    // Caret line: spaces up to (col-1), then ^
    int caret_pos = loc.col;
    if (caret_pos < 1) caret_pos = 1;

    std::cerr << "  "; 
    for (int i = 1; i < caret_pos; ++i) {
        std::cerr << ' ';
    }
    std::cerr << level_colour(level) << "^" << RESET << "\n";

    return false;
}