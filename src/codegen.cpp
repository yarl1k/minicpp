#include "codegen.hpp"

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace mini {
namespace {

constexpr const char* kRuntimeDiv = "__minicpp_runtime_cxx_div";
constexpr const char* kRuntimeMod = "__minicpp_runtime_cxx_mod";

std::string formatDouble(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << std::defaultfloat << value;

    std::string result = out.str();
    // Python treats 3 as an integer and 3.0 as a float. Preserve the
    // MiniCpp literal's declared type even when its value is whole.
    if (result.find('.') == std::string::npos &&
        result.find('e') == std::string::npos &&
        result.find('E') == std::string::npos) {
        result += ".0";
    }
    return result;
}

} // namespace

void PythonCodeGenerator::line(int indent, const std::string& value) {
    output_.append(static_cast<std::size_t>(indent) * 4, ' ');
    output_ += value;
    output_ += '\n';
}

std::string PythonCodeGenerator::declareName(const std::string& name) {
    if (scopes_.empty()) scopes_.emplace_back();

    // Generated names are valid Python identifiers and cannot be Python
    // keywords, built-ins, or names reserved by this generator.
    const std::string generated = "__minicpp_var_" + std::to_string(++uniqueId_);
    scopes_.back()[name] = generated;
    return generated;
}

std::string PythonCodeGenerator::resolveName(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        const auto found = it->find(name);
        if (found != it->end()) return found->second;
    }

    // A successful semantic-analysis pass should make this impossible.
    throw std::logic_error("Python code generator: unresolved variable '" + name + "'");
}

std::string PythonCodeGenerator::resolveFunctionName(const std::string& name) const {
    const auto found = functionNames_.find(name);
    if (found != functionNames_.end()) return found->second;

    // A successful semantic-analysis pass should make this impossible.
    throw std::logic_error("Python code generator: unresolved function '" + name + "'");
}

std::string PythonCodeGenerator::emitExpression(const Expr& expression) const {
    if (const auto* number = dynamic_cast<const NumberExpr*>(&expression)) {
        if (number->isInteger) {
            // integerValue is stored separately in the AST to avoid losing
            // precision by converting a 64-bit integer through double.
            return std::to_string(number->integerValue);
        }
        return formatDouble(number->value);
    }

    if (const auto* boolean = dynamic_cast<const BoolExpr*>(&expression)) {
        return boolean->value ? "True" : "False";
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        return resolveName(variable->name);
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        const std::string operand = emitExpression(*unary->operand);
        if (unary->op == "!") return "(not " + operand + ")";
        if (unary->op == "-") return "(-" + operand + ")";
        throw std::logic_error("Python code generator: unsupported unary operator '" + unary->op + "'");
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        const std::string left = emitExpression(*binary->left);
        const std::string right = emitExpression(*binary->right);
        const std::string& op = binary->op;

        // MiniCpp's semantic analyzer requires bool operands for && and ||,
        // so Python's short-circuit operators also produce bool values here.
        if (op == "&&") return "(" + left + " and " + right + ")";
        if (op == "||") return "(" + left + " or " + right + ")";
        if (op == "/") return std::string(kRuntimeDiv) + "(" + left + ", " + right + ")";
        if (op == "%") return std::string(kRuntimeMod) + "(" + left + ", " + right + ")";

        if (op == "+" || op == "-" || op == "*" ||
            op == "==" || op == "!=" || op == "<" || op == ">" ||
            op == "<=" || op == ">=") {
            return "(" + left + " " + op + " " + right + ")";
        }

        throw std::logic_error("Python code generator: unsupported binary operator '" + op + "'");
    }

    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        const std::string functionName = call->name == "print"
            ? "print"
            : resolveFunctionName(call->name);
        std::string result = functionName + "(";
        for (std::size_t i = 0; i < call->arguments.size(); ++i) {
            if (i != 0) result += ", ";
            result += emitExpression(*call->arguments[i]);
        }
        result += ")";
        return result;
    }

    throw std::logic_error("Python code generator: unsupported expression node");
}

void PythonCodeGenerator::emitBlock(const BlockStmt& block, int indent, bool createScope) {
    if (createScope) scopes_.emplace_back();

    if (block.statements.empty()) {
        line(indent, "pass");
    } else {
        for (const auto& statement : block.statements) {
            emitStatement(*statement, indent);
        }
    }

    if (createScope) scopes_.pop_back();
}

void PythonCodeGenerator::emitSuite(const Stmt& statement, int indent) {
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        emitBlock(*block, indent, true);
    } else {
        // A single-statement suite does not introduce a Python indentation
        // block of its own. The source language's semantic rules are handled
        // by the analyzer; names remain uniquely mangled either way.
        emitStatement(statement, indent);
    }
}

void PythonCodeGenerator::emitStatement(const Stmt& statement, int indent) {
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        emitBlock(*block, indent, true);
        return;
    }

    if (const auto* declaration = dynamic_cast<const VarDeclStmt*>(&statement)) {
        // Register the new name before translating its initializer, matching
        // the point of declaration used by the semantic analyzer.
        const std::string target = declareName(declaration->name);
        std::string initial;

        if (declaration->initializer) {
            initial = emitExpression(*declaration->initializer);
        } else {
            switch (declaration->type) {
                case Type::Bool: initial = "False"; break;
                case Type::Double: initial = "0.0"; break;
                case Type::Int: initial = "0"; break;
                case Type::Void:
                    throw std::logic_error("Python code generator: void variable reached code generation");
            }
        }

        line(indent, target + " = " + initial);
        return;
    }

    if (const auto* assignment = dynamic_cast<const AssignStmt*>(&statement)) {
        line(indent, resolveName(assignment->name) + " = " + emitExpression(*assignment->value));
        return;
    }

    if (const auto* conditional = dynamic_cast<const IfStmt*>(&statement)) {
        line(indent, "if " + emitExpression(*conditional->condition) + ":");
        emitSuite(*conditional->thenBranch, indent + 1);
        if (conditional->elseBranch) {
            line(indent, "else:");
            emitSuite(*conditional->elseBranch, indent + 1);
        }
        return;
    }

    if (const auto* loop = dynamic_cast<const WhileStmt*>(&statement)) {
        line(indent, "while " + emitExpression(*loop->condition) + ":");
        emitSuite(*loop->body, indent + 1);
        return;
    }

    if (const auto* loop = dynamic_cast<const ForStmt*>(&statement)) {
        // C-style for is translated into initialization + while + step.
        // The dedicated scope keeps the initializer variable local to the loop.
        scopes_.emplace_back();
        if (loop->initializer) emitStatement(*loop->initializer, indent);

        const std::string condition = loop->condition
            ? emitExpression(*loop->condition)
            : "True";
        line(indent, "while " + condition + ":");
        emitSuite(*loop->body, indent + 1);
        if (loop->step) emitStatement(*loop->step, indent + 1);
        scopes_.pop_back();
        return;
    }

    if (const auto* ret = dynamic_cast<const ReturnStmt*>(&statement)) {
        line(indent, ret->value ? "return " + emitExpression(*ret->value) : "return");
        return;
    }

    if (const auto* expr = dynamic_cast<const ExprStmt*>(&statement)) {
        line(indent, emitExpression(*expr->expression));
        return;
    }

    throw std::logic_error("Python code generator: unsupported statement node");
}

void PythonCodeGenerator::emitFunction(const FunctionDecl& function) {
    scopes_.clear();
    scopes_.emplace_back();

    std::string signature = "def " + resolveFunctionName(function.name) + "(";
    for (std::size_t i = 0; i < function.parameters.size(); ++i) {
        if (i != 0) signature += ", ";
        signature += declareName(function.parameters[i].name);
    }
    signature += "):";

    line(0, signature);
    emitBlock(*function.body, 1, false);
    line(0, "");
}

std::string PythonCodeGenerator::generate(const Program& program) {
    output_.clear();
    uniqueId_ = 0;
    scopes_.clear();
    functionNames_.clear();

    // Allocate names for every function before emitting any body. This makes
    // recursion and calls to functions defined later work naturally.
    for (std::size_t i = 0; i < program.functions.size(); ++i) {
        const auto& function = program.functions[i];
        functionNames_[function->name] = "__minicpp_fn_" + std::to_string(i);
    }

    line(0, "# -*- coding: utf-8 -*-");
    line(0, "# Generated by MiniCpp Compiler. Do not edit by hand.");
    line(0, "");

    line(0, "def " + std::string(kRuntimeDiv) + "(a, b):");
    line(1, "if isinstance(a, int) and not isinstance(a, bool) and isinstance(b, int) and not isinstance(b, bool):");
    line(2, "quotient = abs(a) // abs(b)");
    line(2, "return -quotient if (a < 0) != (b < 0) else quotient");
    line(1, "return a / b");
    line(0, "");

    line(0, "def " + std::string(kRuntimeMod) + "(a, b):");
    line(1, "remainder = abs(a) % abs(b)");
    line(1, "return -remainder if a < 0 else remainder");
    line(0, "");

    for (const auto& function : program.functions) {
        emitFunction(*function);
    }

    line(0, "if __name__ == \"__main__\":");
    const auto mainName = functionNames_.find("main");
    if (mainName == functionNames_.end()) {
        throw std::logic_error("Python code generator: program has no main() function");
    }
    line(1, "__minicpp_result = " + mainName->second + "()");
    line(1, "if __minicpp_result is not None:");
    line(2, "print(__minicpp_result)");

    return output_;
}

} // namespace mini
