#pragma once
#include "ast.hpp"
#include <ostream>

// Generates a DOT format graph description
void generate_dot(const Program& p, std::ostream& os);