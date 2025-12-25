cmake_minimum_required(VERSION 3.20)
project(rivet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rivet
  src/main.cpp
  src/source.cpp
  src/diag.cpp
  src/lexer.cpp
  src/parser.cpp
  src/print_ast.cpp
  src/validate.cpp
)

if (MINGW)
  target_link_options(rivet PRIVATE "-mconsole")
endif()
