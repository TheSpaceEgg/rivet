#include "diag.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "print_ast.hpp"
#include "source.hpp"
#include "validate.hpp"
#include "graphviz.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>

static std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: rivet <file.rv> [--graph | --show]\n";
        return 1;
    }

    std::string filename = argv[1];
    bool raw_dot_mode = false;
    bool auto_show_mode = false;

    if (argc >= 3) {
        if (std::strcmp(argv[2], "--graph") == 0) raw_dot_mode = true;
        if (std::strcmp(argv[2], "--show") == 0) auto_show_mode = true;
    }

    try {
        Source src(filename, read_file(filename));
        DiagnosticEngine diag(src);
        Lexer lex(src, diag);
        Parser parser(lex, diag);

        Program p = parser.parse_program();
        bool ok = validate_program(p, diag);

        if (!ok) return 2;

        if (raw_dot_mode) {
            // Output raw DOT text to stdout (for piping)
            generate_dot(p, std::cout);
        } 
        else if (auto_show_mode) {
            // Generate HTML and pop it open
            generate_and_open_html(p, "rivet_graph.html");
        } 
        else {
            // Default: AST
            print_ast(p, std::cout);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}