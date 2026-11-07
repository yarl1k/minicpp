#pragma once

#include "ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace mini {
struct Diagnostic { int line; std::string message; };

class SemanticAnalyzer {
public:
    bool analyze(const Program& program);
    const std::vector<Diagnostic>& errors() const { return errors_; }
private:
    struct FunctionSignature { Type returnType; std::vector<Type> parameters; };
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::vector<std::unordered_map<std::string, Type>> scopes_;
    std::vector<Diagnostic> errors_;
    Type currentReturnType_ = Type::Void;
    bool insideFunction_ = false;

    void error(int line, const std::string& message);
    Type checkExpression(const Expr& expression);
    void checkStatement(const Stmt& statement);
    void checkBlock(const BlockStmt& block, bool createScope = true);
    bool definitelyReturns(const Stmt& statement) const;
    Type lookupVariable(const std::string& name, int line);
    bool compatible(Type target, Type source) const;
    static bool numeric(Type type);
};
}
