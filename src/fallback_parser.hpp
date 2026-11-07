#pragma once

#include "ast.hpp"
#include <memory>
#include <string>
#include <vector>

namespace mini {
struct FallbackParseOutput {
    std::unique_ptr<Program> program;
    std::vector<std::string> errors;
};
FallbackParseOutput parseWithFallbackParser(const std::string& source);
}
