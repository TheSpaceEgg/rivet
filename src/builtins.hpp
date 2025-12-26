#pragma once
#include <string_view>

// Built-in functions that are recognised by the compiler.
//
// These are *not* user-defined node functions. They exist to provide language-level
// functionality (and let you add more without touching the parser).

enum class BuiltinId {
    Min,
    Max,
    Clamp,
};

// Returns nullptr if name is not a builtin.
const BuiltinId* lookup_builtin(std::string_view name);
