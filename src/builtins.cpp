#include "builtins.hpp"

const BuiltinId* lookup_builtin(std::string_view name) {
    static const BuiltinId kMin = BuiltinId::Min;
    static const BuiltinId kMax = BuiltinId::Max;
    static const BuiltinId kClamp = BuiltinId::Clamp;

    if (name == "min") return &kMin;
    if (name == "max") return &kMax;
    if (name == "clamp") return &kClamp;
    return nullptr;
}
