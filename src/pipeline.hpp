#pragma once

#include "interpreter.hpp"
#include "semantic.hpp"
#include <memory>
#include <string>
#include <vector>

namespace mini {
struct CompileResult {
    bool parsed = false;
    bool semanticOk = false;
    std::vector<std::string> syntaxErrors;
    std::vector<Diagnostic> semanticErrors;
    std::string runtimeError;
    std::string astJson;
    std::string pythonCode;
    std::optional<Value> result;
};
CompileResult compileSource(const std::string& source, bool executeProgram = true);
std::string compileResultToJson(const CompileResult& result);
}
