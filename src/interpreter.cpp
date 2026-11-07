#include "interpreter.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace mini {

RuntimeError::RuntimeError(int line, const std::string& message)
    : std::runtime_error("line " + std::to_string(line) + ": " + message), line_(line) {}

Interpreter::Interpreter(const Program& program) : program_(program) {
    for (const auto& function : program.functions) functions_[function->name] = function.get();
}

bool Interpreter::isInteger(const Value& value) { return std::holds_alternative<std::int64_t>(value); }
bool Interpreter::isNumeric(const Value& value) { return isInteger(value) || std::holds_alternative<double>(value); }
double Interpreter::asDouble(const Value& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) return static_cast<double>(*v);
    if (auto* v = std::get_if<double>(&value)) return *v;
    throw std::runtime_error("expected numeric value");
}
std::int64_t Interpreter::asInteger(const Value& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) return *v;
    throw std::runtime_error("expected integer value");
}
bool Interpreter::asBool(const Value& value) {
    if (auto* v = std::get_if<bool>(&value)) return *v;
    throw std::runtime_error("expected bool value");
}
Value Interpreter::defaultValue(Type type) {
    switch (type) {
        case Type::Int: return std::int64_t{0};
        case Type::Double: return 0.0;
        case Type::Bool: return false;
        case Type::Void: break;
    }
    throw std::runtime_error("void variable has no value");
}

std::string Interpreter::valueToString(const Value& value) {
    if (auto* v = std::get_if<std::int64_t>(&value)) return std::to_string(*v);
    if (auto* v = std::get_if<double>(&value)) {
        std::ostringstream out; out << std::setprecision(12) << *v; return out.str();
    }
    // Match Python's bool spelling so interpreter and generated code agree.
    return std::get<bool>(value) ? "True" : "False";
}

std::optional<Value> Interpreter::run() {
    return callFunction("main", {}, 1);
}

std::optional<Value> Interpreter::callFunction(const std::string& name, const std::vector<Value>& args, int line) {
    auto found = functions_.find(name);
    if (found == functions_.end()) throw RuntimeError(line, "unknown function '" + name + "'");
    const FunctionDecl& function = *found->second;
    if (args.size() != function.parameters.size()) throw RuntimeError(line, "wrong number of arguments for function '" + name + "'");
    if (++callDepth_ > 512) { --callDepth_; throw RuntimeError(line, "maximum function call depth exceeded"); }

    scopes_.emplace_back();
    for (std::size_t i = 0; i < args.size(); ++i) scopes_.back()[function.parameters[i].name] = args[i];
    ExecResult result;
    try {
        result = executeBlock(*function.body, false);
    } catch (...) {
        scopes_.pop_back(); --callDepth_; throw;
    }
    scopes_.pop_back(); --callDepth_;
    return result.value;
}

Value Interpreter::getVariable(const std::string& name, int line) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    throw RuntimeError(line, "unknown variable '" + name + "'");
}
void Interpreter::setVariable(const std::string& name, Value value, int line) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) { found->second = std::move(value); return; }
    }
    throw RuntimeError(line, "assignment to unknown variable '" + name + "'");
}

Value Interpreter::evaluate(const Expr& expression) {
    if (auto* number = dynamic_cast<const NumberExpr*>(&expression)) {
        if (number->isInteger) return number->integerValue;
        return number->value;
    }
    if (auto* boolean = dynamic_cast<const BoolExpr*>(&expression)) return boolean->value;
    if (auto* variable = dynamic_cast<const VariableExpr*>(&expression)) return getVariable(variable->name, variable->line);
    if (auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        Value value = evaluate(*unary->operand);
        if (unary->op == "-") {
            if (isInteger(value)) return -asInteger(value);
            if (std::holds_alternative<double>(value)) return -asDouble(value);
            throw RuntimeError(unary->line, "unary '-' requires a number");
        }
        if (unary->op == "!") return !asBool(value);
    }
    if (auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        Value left = evaluate(*binary->left);
        if (binary->op == "&&") return asBool(left) && asBool(evaluate(*binary->right));
        if (binary->op == "||") return asBool(left) || asBool(evaluate(*binary->right));
        Value right = evaluate(*binary->right);
        const std::string& op = binary->op;
        if (op == "+" || op == "-" || op == "*" || op == "/") {
            if (!isNumeric(left) || !isNumeric(right)) throw RuntimeError(binary->line, "numeric operands expected");
            if (isInteger(left) && isInteger(right)) {
                auto a = asInteger(left), b = asInteger(right);
                if (op == "+") return a + b;
                if (op == "-") return a - b;
                if (op == "*") return a * b;
                if (b == 0) throw RuntimeError(binary->line, "division by zero");
                return a / b;
            }
            double a = asDouble(left), b = asDouble(right);
            if (op == "+") return a + b;
            if (op == "-") return a - b;
            if (op == "*") return a * b;
            if (b == 0.0) throw RuntimeError(binary->line, "division by zero");
            return a / b;
        }
        if (op == "%") {
            auto a = asInteger(left), b = asInteger(right);
            if (b == 0) throw RuntimeError(binary->line, "modulo by zero");
            return a % b;
        }
        if (op == "==" || op == "!=") {
            bool equal;
            if (isNumeric(left) && isNumeric(right)) equal = asDouble(left) == asDouble(right);
            else if (std::holds_alternative<bool>(left) && std::holds_alternative<bool>(right)) equal = asBool(left) == asBool(right);
            else throw RuntimeError(binary->line, "incompatible values in equality comparison");
            return op == "==" ? equal : !equal;
        }
        if (op == "<") return asDouble(left) < asDouble(right);
        if (op == ">") return asDouble(left) > asDouble(right);
        if (op == "<=") return asDouble(left) <= asDouble(right);
        if (op == ">=") return asDouble(left) >= asDouble(right);
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        if (call->name == "print") {
            throw RuntimeError(call->line, "void function 'print' used as an expression");
        }
        std::vector<Value> args;
        for (const auto& arg : call->arguments) args.push_back(evaluate(*arg));
        auto result = callFunction(call->name, args, call->line);
        if (!result) throw RuntimeError(call->line, "void function '" + call->name + "' used as an expression");
        return *result;
    }
    throw RuntimeError(expression.line, "unsupported expression");
}

Interpreter::ExecResult Interpreter::executeBlock(const BlockStmt& block, bool createScope) {
    if (createScope) scopes_.emplace_back();
    for (const auto& statement : block.statements) {
        ExecResult result = execute(*statement);
        if (result.returned) { if (createScope) scopes_.pop_back(); return result; }
    }
    if (createScope) scopes_.pop_back();
    return {};
}

Interpreter::ExecResult Interpreter::execute(const Stmt& statement) {
    if (auto* block = dynamic_cast<const BlockStmt*>(&statement)) return executeBlock(*block, true);
    if (auto* declaration = dynamic_cast<const VarDeclStmt*>(&statement)) {
        Value value = declaration->initializer ? evaluate(*declaration->initializer) : defaultValue(declaration->type);
        if (declaration->type == Type::Double && isInteger(value)) value = static_cast<double>(asInteger(value));
        if (declaration->type == Type::Int && std::holds_alternative<double>(value)) value = static_cast<std::int64_t>(asDouble(value));
        if (declaration->type == Type::Bool && !std::holds_alternative<bool>(value)) throw RuntimeError(declaration->line, "bool initializer expected");
        scopes_.back()[declaration->name] = std::move(value); return {};
    }
    if (auto* assignment = dynamic_cast<const AssignStmt*>(&statement)) {
        Value value = evaluate(*assignment->value);
        setVariable(assignment->name, std::move(value), assignment->line); return {};
    }
    if (auto* conditional = dynamic_cast<const IfStmt*>(&statement)) {
        if (asBool(evaluate(*conditional->condition))) return execute(*conditional->thenBranch);
        if (conditional->elseBranch) return execute(*conditional->elseBranch);
        return {};
    }
    if (auto* loop = dynamic_cast<const WhileStmt*>(&statement)) {
        std::size_t iterations = 0;
        while (asBool(evaluate(*loop->condition))) {
            if (++iterations > 1000000) throw RuntimeError(loop->line, "while loop exceeded 1,000,000 iterations");
            ExecResult result = execute(*loop->body); if (result.returned) return result;
        }
        return {};
    }
    if (auto* loop = dynamic_cast<const ForStmt*>(&statement)) {
        scopes_.emplace_back();
        if (loop->initializer) execute(*loop->initializer);
        std::size_t iterations = 0;
        while (!loop->condition || asBool(evaluate(*loop->condition))) {
            if (++iterations > 1000000) { scopes_.pop_back(); throw RuntimeError(loop->line, "for loop exceeded 1,000,000 iterations"); }
            ExecResult result = execute(*loop->body);
            if (result.returned) { scopes_.pop_back(); return result; }
            if (loop->step) execute(*loop->step);
        }
        scopes_.pop_back(); return {};
    }
    if (auto* ret = dynamic_cast<const ReturnStmt*>(&statement)) {
        if (ret->value) return {true, evaluate(*ret->value)};
        return {true, std::nullopt};
    }
    if (auto* expr = dynamic_cast<const ExprStmt*>(&statement)) {
        if (auto* call = dynamic_cast<const CallExpr*>(expr->expression.get())) {
            std::vector<Value> args;
            for (const auto& arg : call->arguments) args.push_back(evaluate(*arg));
            if (call->name == "print") {
                if (args.size() != 1) throw RuntimeError(call->line, "function 'print' expects one argument");
                std::cout << valueToString(args.front()) << '\n';
            } else {
                (void)callFunction(call->name, args, call->line);
            }
        } else {
            (void)evaluate(*expr->expression);
        }
        return {};
    }
    throw RuntimeError(statement.line, "unsupported statement");
}

} // namespace mini
