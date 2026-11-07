#pragma once

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace mini
{

    enum class Type
    {
        Int,
        Double,
        Bool,
        Void
    };
    std::string typeName(Type type);
    std::string jsonEscape(const std::string &value);

    struct Node
    {
        explicit Node(int line = 1) : line(line) {}
        virtual ~Node() = default;
        int line;
        virtual void writeJson(std::ostream &out) const = 0;
    };

    struct Expr : Node
    {
        using Node::Node;
    };
    struct Stmt : Node
    {
        using Node::Node;
    };

    struct NumberExpr final : Expr
    {
        NumberExpr(double value, bool isInteger, int line);
        NumberExpr(std::int64_t integerValue, int line);
        double value;
        std::int64_t integerValue = 0;
        bool isInteger;
        void writeJson(std::ostream &out) const override;
    };

    struct BoolExpr final : Expr
    {
        BoolExpr(bool value, int line);
        bool value;
        void writeJson(std::ostream &out) const override;
    };

    struct VariableExpr final : Expr
    {
        VariableExpr(std::string name, int line);
        std::string name;
        void writeJson(std::ostream &out) const override;
    };

    struct UnaryExpr final : Expr
    {
        UnaryExpr(std::string op, Expr *operand, int line);
        std::string op;
        std::unique_ptr<Expr> operand;
        void writeJson(std::ostream &out) const override;
    };

    struct BinaryExpr final : Expr
    {
        BinaryExpr(std::string op, Expr *left, Expr *right, int line);
        std::string op;
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        void writeJson(std::ostream &out) const override;
    };

    struct CallExpr final : Expr
    {
        CallExpr(std::string name, std::vector<Expr *> *arguments, int line);
        std::string name;
        std::vector<std::unique_ptr<Expr>> arguments;
        void writeJson(std::ostream &out) const override;
    };

    struct BlockStmt final : Stmt
    {
        BlockStmt(std::vector<Stmt *> *statements, int line);
        std::vector<std::unique_ptr<Stmt>> statements;
        void writeJson(std::ostream &out) const override;
    };

    struct VarDeclStmt final : Stmt
    {
        VarDeclStmt(Type type, std::string name, Expr *initializer, int line);
        Type type;
        std::string name;
        std::unique_ptr<Expr> initializer;
        void writeJson(std::ostream &out) const override;
    };

    struct AssignStmt final : Stmt
    {
        AssignStmt(std::string name, Expr *value, int line);
        std::string name;
        std::unique_ptr<Expr> value;
        void writeJson(std::ostream &out) const override;
    };

    struct IfStmt final : Stmt
    {
        IfStmt(Expr *condition, Stmt *thenBranch, Stmt *elseBranch, int line);
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Stmt> thenBranch;
        std::unique_ptr<Stmt> elseBranch;
        void writeJson(std::ostream &out) const override;
    };

    struct WhileStmt final : Stmt
    {
        WhileStmt(Expr *condition, Stmt *body, int line);
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Stmt> body;
        void writeJson(std::ostream &out) const override;
    };

    struct ForStmt final : Stmt
    {
        ForStmt(Stmt *initializer, Expr *condition, Stmt *step, Stmt *body, int line);
        std::unique_ptr<Stmt> initializer;
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Stmt> step;
        std::unique_ptr<Stmt> body;
        void writeJson(std::ostream &out) const override;
    };

    struct ReturnStmt final : Stmt
    {
        ReturnStmt(Expr *value, int line);
        std::unique_ptr<Expr> value;
        void writeJson(std::ostream &out) const override;
    };

    struct ExprStmt final : Stmt
    {
        ExprStmt(Expr *expression, int line);
        std::unique_ptr<Expr> expression;
        void writeJson(std::ostream &out) const override;
    };

    struct Parameter
    {
        Type type;
        std::string name;
        int line;
    };

    struct FunctionDecl final : Node
    {
        FunctionDecl(Type returnType, std::string name, std::vector<Parameter> *parameters, BlockStmt *body, int line);
        Type returnType;
        std::string name;
        std::vector<Parameter> parameters;
        std::unique_ptr<BlockStmt> body;
        void writeJson(std::ostream &out) const override;
    };

    struct Program final : Node
    {
        explicit Program(std::vector<FunctionDecl *> *functions);
        std::vector<std::unique_ptr<FunctionDecl>> functions;
        void writeJson(std::ostream &out) const override;
    };

    std::string astToJson(const Program &program);

} // namespace mini
