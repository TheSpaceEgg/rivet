#include "source.hpp"
#include <algorithm>

Source::Source(std::string filename, std::string text)
    : filename_(std::move(filename)), text_(std::move(text)) {
    line_starts_.clear();
    line_starts_.push_back(0); // line 1 starts at offset 0

    for (int i = 0; i < static_cast<int>(text_.size()); ++i) {
        if (text_[i] == '\n') {
            int next = i + 1;
            if (next <= static_cast<int>(text_.size())) {
                line_starts_.push_back(next);
            }
        }
    }

    // If file ends with a newline, the last start may equal text_.size().
    // That's fine; it represents an empty last line.
}

SourceLoc Source::loc_from_offset(int offset) const {
    if (offset < 0) offset = 0;
    if (offset > static_cast<int>(text_.size())) offset = static_cast<int>(text_.size());

    // Find the last line start <= offset
    auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
    int line_index = static_cast<int>(it - line_starts_.begin()) - 1;
    if (line_index < 0) line_index = 0;

    int line_start = line_starts_[line_index];
    int col = (offset - line_start) + 1;

    SourceLoc loc;
    loc.offset = offset;
    loc.line = line_index + 1; // 1-based
    loc.col = col;
    return loc;
}

std::string_view Source::line_text(int line1) const {
    if (line1 < 1) return {};
    int idx = line1 - 1;
    if (idx >= static_cast<int>(line_starts_.size())) return {};

    int start = line_starts_[idx];
    int end = static_cast<int>(text_.size());

    // End at next line start - 1 (excluding newline), if there is a next line.
    if (idx + 1 < static_cast<int>(line_starts_.size())) {
        end = line_starts_[idx + 1];
        // trim trailing '\n'
        if (end > start && text_[end - 1] == '\n') end -= 1;
        // also trim '\r' for Windows line endings
        if (end > start && text_[end - 1] == '\r') end -= 1;
    } else {
        // last line: trim possible trailing '\r' or '\n' just in case
        while (end > start && (text_[end - 1] == '\n' || text_[end - 1] == '\r')) end -= 1;
    }

    return std::string_view(text_).substr(static_cast<size_t>(start),
                                          static_cast<size_t>(end - start));
}
