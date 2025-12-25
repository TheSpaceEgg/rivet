#include "diag.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "print_ast.hpp"
#include "source.hpp"
#include "validate.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: rivet <file.rv>\n";
        return 1;
    }

    try {
        Source src(argv[1], read_file(argv[1]));
        DiagnosticEngine diag(src);
        Lexer lex(src, diag);
        Parser parser(lex, diag);

        Program p = parser.parse_program();

        // NEW: semantic checks (unknown nodes, etc.)
        bool ok = validate_program(p, diag);

        print_ast(p, std::cout);
        return ok ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
