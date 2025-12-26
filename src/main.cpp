#include "diag.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "print_ast.hpp"
#include "source.hpp"
#include "validate.hpp"
#include "graphviz.hpp" // <--- Include this

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring> // for strcmp

static std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: rivet <file.rv> [--graph]\n";
        return 1;
    }

    std::string filename = argv[1];
    bool graph_mode = false;

    // Simple arg parsing
    if (argc >= 3 && std::strcmp(argv[2], "--graph") == 0) {
        graph_mode = true;
    }

    try {
        Source src(filename, read_file(filename));
        DiagnosticEngine diag(src);
        Lexer lex(src, diag);
        Parser parser(lex, diag);

        Program p = parser.parse_program();
        bool ok = validate_program(p, diag);

        if (!ok) return 2;

        if (graph_mode) {
            generate_dot(p, std::cout);
        } else {
            print_ast(p, std::cout);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}