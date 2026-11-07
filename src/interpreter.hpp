#pragma once

#include "ast.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mini {
using Value = std::variant<std::int64_t, double, bool>;

class RuntimeError : public std::runtime_error {
public:
    RuntimeError(int line, const std::string& message);
    int line() const { return line_; }
private:
    int line_;
};

class Interpreter {
public:
    explicit Interpreter(const Program& program);
    std::optional<Value> run();
    static std::string valueToString(const Value& value);
private:
    struct ExecResult { bool returned = false; std::optional<Value> value; };
    const Program& program_;
    std::unordered_map<std::string, const FunctionDecl*> functions_;
    std::vector<std::unordered_map<std::string, Value>> scopes_;
    int callDepth_ = 0;

    std::optional<Value> callFunction(const std::string& name, const std::vector<Value>& args, int line);
    Value evaluate(const Expr& expression);
    ExecResult execute(const Stmt& statement);
    ExecResult executeBlock(const BlockStmt& block, bool createScope = true);
    Value getVariable(const std::string& name, int line) const;
    void setVariable(const std::string& name, Value value, int line);
    static bool isInteger(const Value& value);
    static bool isNumeric(const Value& value);
    static double asDouble(const Value& value);
    static std::int64_t asInteger(const Value& value);
    static bool asBool(const Value& value);
    static Value defaultValue(Type type);
};
}
