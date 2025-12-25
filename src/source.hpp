#pragma once
#include <string>
#include <string_view>
#include <vector>

struct SourceLoc {
    int offset = 0; // 0-based byte offset into the source text
    int line = 1;   // 1-based
    int col = 1;    // 1-based
};

class Source {
public:
    Source(std::string filename, std::string text);

    const std::string& filename() const { return filename_; }
    const std::string& text() const { return text_; }

    // Convert an offset (0-based) to line/col (1-based).
    SourceLoc loc_from_offset(int offset) const;

    // Return the text of a 1-based line number (without newline).
    std::string_view line_text(int line1) const;

    int line_count() const { return static_cast<int>(line_starts_.size()); }

private:
    std::string filename_;
    std::string text_;
    std::vector<int> line_starts_; // offsets of each line start (0-based)
};
