#pragma once

#include "ast.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace mini
{

    // Translates the supported MiniCpp AST to executable Python 3 code.
    // All source identifiers are mangled so Python keywords, built-ins and
    // runtime helper names cannot collide with user-defined names.
    class PythonCodeGenerator
    {
    public:
        std::string generate(const Program &program);

    private:
        std::string output_;
        int uniqueId_ = 0;

        // One map per lexical scope: source name -> generated Python name.
        std::vector<std::unordered_map<std::string, std::string>> scopes_;

        // Function names are resolved independently from local variable names.
        std::unordered_map<std::string, std::string> functionNames_;

        void line(int indent, const std::string &value);
        void emitFunction(const FunctionDecl &function);
        void emitBlock(const BlockStmt &block, int indent, bool createScope);
        void emitSuite(const Stmt &statement, int indent);
        void emitStatement(const Stmt &statement, int indent);

        std::string emitExpression(const Expr &expression) const;
        std::string declareName(const std::string &name);
        std::string resolveName(const std::string &name) const;
        std::string resolveFunctionName(const std::string &name) const;
    };

} // namespace mini
