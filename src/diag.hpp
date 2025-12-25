#pragma once
#include "source.hpp"
#include <string>

enum class DiagLevel { Error, Warning, Note };

struct Diag {
    DiagLevel level = DiagLevel::Error;
    SourceLoc loc;
    std::string message;
};

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(const Source& src) : src_(src) {}

    // Prints the diagnostic and returns false (handy for "return diag.error(...)")
    bool report(DiagLevel level, SourceLoc loc, const std::string& message) const;

    bool error(SourceLoc loc, const std::string& message) const {
        return report(DiagLevel::Error, loc, message);
    }

private:
    const Source& src_;
};
