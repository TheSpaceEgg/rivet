#include "diag.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "print_ast.hpp"
#include "source.hpp"
#include "validate.hpp"
#include "graphviz.hpp"
#include "codegen_cpp.hpp"

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
        std::cerr << "Usage: rivet <file.rv> [--graph | --show | --cpp]\n";
        return 1;
    }

    std::string filename = argv[1];
    
    bool raw_dot_mode = false;
    bool auto_show_mode = false;
    bool cpp_mode = false;

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--graph") == 0) raw_dot_mode = true;
        else if (std::strcmp(argv[i], "--show") == 0) auto_show_mode = true;
        else if (std::strcmp(argv[i], "--cpp") == 0) cpp_mode = true;
    }

    try {
        Source src(filename, read_file(filename));
        DiagnosticEngine diag(src);
        Lexer lex(src, diag);
        Parser parser(lex, diag);

        Program p = parser.parse_program();

        // 1. Check for Parse Errors
        // We don't call diag.print() because report() prints immediately
        if (diag.has_errors()) {
            return 1;
        }

        // 2. Validate Logic
        if (!validate_program(p, diag)) {
            return 2;
        }

        // 3. Output
        if (cpp_mode) {
            std::string out_name = filename + ".cpp";
            std::ofstream out(out_name);
            generate_cpp(p, out);
            std::cout << "Generated C++: " << out_name << "\n";
            std::cout << "Compile with: g++ " << out_name << " -o app -std=c++17\n";
        }
        else if (raw_dot_mode) {
            generate_dot(p, std::cout);
        } 
        else if (auto_show_mode) {
            generate_and_open_html(p, "rivet_graph.html");
        } 
        else {
            print_ast(p, std::cout);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}