#pragma once
#include "ast.hpp"
#include <ostream>
#include <string>

// Generates the raw DOT text
void generate_dot(const Program& p, std::ostream& os);

// NEW: Generates an HTML file and opens it in the browser
void generate_and_open_html(const Program& p, const std::string& filename);