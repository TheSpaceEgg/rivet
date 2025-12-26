#pragma once
#include "ast.hpp"
#include <ostream>

// Generates a complete, single-file C++ application from the Rivet program.
void generate_cpp(const Program& p, std::ostream& os);