#pragma once
#include "source.hpp"
#include <string>

enum class DiagLevel { Error, Warning, Note };

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(const Source& src) : src_(src) {}

    // ADDED: const here
    bool report(DiagLevel level, SourceLoc loc, const std::string& message) const;

    // ADDED: const here
    bool error(SourceLoc loc, const std::string& message) const {
        return report(DiagLevel::Error, loc, message);
    }

    bool has_errors() const { return had_error_; }

private:
    const Source& src_;
    // ADDED: mutable here (allows const methods to change this specific flag)
    mutable bool had_error_ = false;
};