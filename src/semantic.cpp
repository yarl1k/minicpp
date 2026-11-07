#include "semantic.hpp"

#include <algorithm>

namespace mini {

bool SemanticAnalyzer::numeric(Type type) { return type == Type::Int || type == Type::Double; }
bool SemanticAnalyzer::compatible(Type target, Type source) const {
    // Keep assignment/argument/return conversions explicit in this teaching subset.
    return target == source;
}
void SemanticAnalyzer::error(int line, const std::string& message) { errors_.push_back({line, message}); }

Type SemanticAnalyzer::lookupVariable(const std::string& name, int line) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    error(line, "use of undeclared variable '" + name + "'");
    return Type::Int;
}

Type SemanticAnalyzer::checkExpression(const Expr& expression) {
    if (auto* number = dynamic_cast<const NumberExpr*>(&expression))
        return number->isInteger ? Type::Int : Type::Double;
    if (dynamic_cast<const BoolExpr*>(&expression)) return Type::Bool;
    if (auto* variable = dynamic_cast<const VariableExpr*>(&expression))
        return lookupVariable(variable->name, variable->line);
    if (auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        Type operand = checkExpression(*unary->operand);
        if (unary->op == "-") {
            if (!numeric(operand)) error(unary->line, "unary '-' requires a numeric operand");
            return operand;
        }
        if (unary->op == "!") {
            if (operand != Type::Bool) error(unary->line, "operator '!' requires a bool operand");
            return Type::Bool;
        }
    }
    if (auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        Type left = checkExpression(*binary->left);
        Type right = checkExpression(*binary->right);
        const std::string& op = binary->op;
        if (op == "+" || op == "-" || op == "*" || op == "/") {
            if (!numeric(left) || !numeric(right)) error(binary->line, "operator '" + op + "' requires numeric operands");
            return (left == Type::Double || right == Type::Double) ? Type::Double : Type::Int;
        }
        if (op == "%") {
            if (left != Type::Int || right != Type::Int) error(binary->line, "operator '%' requires int operands");
            return Type::Int;
        }
        if (op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (!numeric(left) || !numeric(right)) error(binary->line, "comparison '" + op + "' requires numeric operands");
            return Type::Bool;
        }
        if (op == "==" || op == "!=") {
            if (!(compatible(left, right) || (left == Type::Bool && right == Type::Bool)))
                error(binary->line, "operator '" + op + "' received incompatible operand types");
            return Type::Bool;
        }
        if (op == "&&" || op == "||") {
            if (left != Type::Bool || right != Type::Bool)
                error(binary->line, "operator '" + op + "' requires bool operands");
            return Type::Bool;
        }
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        std::vector<Type> actual;
        for (const auto& arg : call->arguments) actual.push_back(checkExpression(*arg));

        // Built-in output function. It is a statement-level operation and
        // therefore returns void, just like a user-defined void function.
        if (call->name == "print") {
            if (actual.size() != 1) {
                error(call->line, "function 'print' expects 1 argument, got " +
                      std::to_string(actual.size()));
            } else if (actual[0] != Type::Int && actual[0] != Type::Double && actual[0] != Type::Bool) {
                error(call->line, "argument of function 'print' must have type int, double or bool");
            }
            return Type::Void;
        }

        auto found = functions_.find(call->name);
        if (found == functions_.end()) {
            error(call->line, "call to undeclared function '" + call->name + "'");
            return Type::Int;
        }
        if (actual.size() != found->second.parameters.size()) {
            error(call->line, "function '" + call->name + "' expects " +
                  std::to_string(found->second.parameters.size()) + " argument(s), got " + std::to_string(actual.size()));
        } else {
            for (std::size_t i = 0; i < actual.size(); ++i) {
                if (!compatible(found->second.parameters[i], actual[i]))
                    error(call->line, "argument " + std::to_string(i + 1) + " of function '" + call->name + "' has incompatible type");
            }
        }
        return found->second.returnType;
    }
    error(expression.line, "unsupported expression");
    return Type::Int;
}

void SemanticAnalyzer::checkBlock(const BlockStmt& block, bool createScope) {
    if (createScope) scopes_.emplace_back();
    for (const auto& statement : block.statements) checkStatement(*statement);
    if (createScope) scopes_.pop_back();
}

bool SemanticAnalyzer::definitelyReturns(const Stmt& statement) const {
    if (dynamic_cast<const ReturnStmt*>(&statement)) return true;
    if (auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        for (const auto& child : block->statements) {
            if (definitelyReturns(*child)) return true;
        }
        return false;
    }
    if (auto* conditional = dynamic_cast<const IfStmt*>(&statement)) {
        return conditional->elseBranch && definitelyReturns(*conditional->thenBranch) &&
               definitelyReturns(*conditional->elseBranch);
    }
    return false;
}

void SemanticAnalyzer::checkStatement(const Stmt& statement) {
    if (auto* block = dynamic_cast<const BlockStmt*>(&statement)) { checkBlock(*block); return; }
    if (auto* declaration = dynamic_cast<const VarDeclStmt*>(&statement)) {
        if (declaration->type == Type::Void) error(declaration->line, "variables cannot have type void");
        if (scopes_.empty()) scopes_.emplace_back();
        auto& scope = scopes_.back();
        if (scope.find(declaration->name) != scope.end())
            error(declaration->line, "redeclaration of variable '" + declaration->name + "' in the same scope");
        else scope[declaration->name] = declaration->type;
        if (declaration->initializer) {
            Type actual = checkExpression(*declaration->initializer);
            if (!compatible(declaration->type, actual))
                error(declaration->line, "cannot initialize '" + declaration->name + "' of type " + typeName(declaration->type) + " with " + typeName(actual));
        }
        return;
    }
    if (auto* assignment = dynamic_cast<const AssignStmt*>(&statement)) {
        Type target = lookupVariable(assignment->name, assignment->line);
        Type source = checkExpression(*assignment->value);
        if (!compatible(target, source)) error(assignment->line, "cannot assign " + typeName(source) + " to variable '" + assignment->name + "' of type " + typeName(target));
        return;
    }
    if (auto* conditional = dynamic_cast<const IfStmt*>(&statement)) {
        Type condition = checkExpression(*conditional->condition);
        if (condition != Type::Bool) error(conditional->line, "if condition must have type bool");
        checkStatement(*conditional->thenBranch);
        if (conditional->elseBranch) checkStatement(*conditional->elseBranch);
        return;
    }
    if (auto* loop = dynamic_cast<const WhileStmt*>(&statement)) {
        Type condition = checkExpression(*loop->condition);
        if (condition != Type::Bool) error(loop->line, "while condition must have type bool");
        checkStatement(*loop->body);
        return;
    }
    if (auto* loop = dynamic_cast<const ForStmt*>(&statement)) {
        scopes_.emplace_back();
        if (loop->initializer) checkStatement(*loop->initializer);
        if (loop->condition) {
            Type condition = checkExpression(*loop->condition);
            if (condition != Type::Bool) error(loop->line, "for condition must have type bool");
        }
        checkStatement(*loop->body);
        if (loop->step) checkStatement(*loop->step);
        scopes_.pop_back();
        return;
    }
    if (auto* ret = dynamic_cast<const ReturnStmt*>(&statement)) {
        if (!insideFunction_) { error(ret->line, "return used outside a function"); return; }
        if (currentReturnType_ == Type::Void && ret->value)
            error(ret->line, "void function cannot return a value");
        else if (currentReturnType_ != Type::Void && !ret->value)
            error(ret->line, "non-void function must return a value");
        else if (ret->value) {
            Type actual = checkExpression(*ret->value);
            if (!compatible(currentReturnType_, actual))
                error(ret->line, "return type mismatch: expected " + typeName(currentReturnType_) + ", got " + typeName(actual));
        }
        return;
    }
    if (auto* expr = dynamic_cast<const ExprStmt*>(&statement)) { (void)checkExpression(*expr->expression); return; }
    error(statement.line, "unsupported statement");
}

bool SemanticAnalyzer::analyze(const Program& program) {
    errors_.clear(); functions_.clear(); scopes_.clear();
    for (const auto& function : program.functions) {
        if (function->name == "print") {
            error(function->line, "'print' is a reserved built-in function name");
            continue;
        }
        if (functions_.count(function->name)) {
            error(function->line, "redefinition of function '" + function->name + "'");
            continue;
        }
        FunctionSignature signature{function->returnType, {}};
        for (const auto& parameter : function->parameters) signature.parameters.push_back(parameter.type);
        functions_[function->name] = std::move(signature);
    }
    if (!functions_.count("main")) error(1, "program must define a main() function");
    else if (!functions_["main"].parameters.empty()) error(1, "main() must not have parameters");

    for (const auto& function : program.functions) {
        scopes_.clear(); scopes_.emplace_back();
        currentReturnType_ = function->returnType;
        insideFunction_ = true;
        for (const auto& parameter : function->parameters) {
            if (parameter.type == Type::Void) error(parameter.line, "parameter cannot have type void");
            if (scopes_.back().count(parameter.name)) error(parameter.line, "duplicate parameter '" + parameter.name + "'");
            else scopes_.back()[parameter.name] = parameter.type;
        }
        checkBlock(*function->body, false);
        if (function->returnType != Type::Void && !definitelyReturns(*function->body))
            error(function->line, "non-void function '" + function->name + "' may finish without returning a value");
    }
    insideFunction_ = false;
    return errors_.empty();
}

} // namespace mini
