#pragma once
#include "ast.hpp"
#include "diag.hpp"

bool validate_program(const Program& program,
                      const DiagnosticEngine& diag);
