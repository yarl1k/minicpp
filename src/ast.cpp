#include "ast.hpp"

#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace mini
{
    namespace
    {
        void writeLine(std::ostream &out, int line) { out << ",\"line\":" << line; }
        void writeExprArray(std::ostream &out, const std::vector<std::unique_ptr<Expr>> &values)
        {
            out << '[';
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (i)
                    out << ',';
                values[i]->writeJson(out);
            }
            out << ']';
        }
    }

    std::string typeName(Type type)
    {
        switch (type)
        {
        case Type::Int:
            return "int";
        case Type::Double:
            return "double";
        case Type::Bool:
            return "bool";
        case Type::Void:
            return "void";
        }
        return "unknown";
    }

    std::string jsonEscape(const std::string &value)
    {
        std::ostringstream out;
        for (unsigned char c : value)
        {
            switch (c)
            {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (c < 0x20)
                {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                }
                else
                    out << static_cast<char>(c);
            }
        }
        return out.str();
    }

    NumberExpr::NumberExpr(double v, bool integer, int ln)
        : Expr(ln), value(v), integerValue(integer ? static_cast<std::int64_t>(v) : 0), isInteger(integer) {}
    NumberExpr::NumberExpr(std::int64_t integer, int ln)
        : Expr(ln), value(static_cast<double>(integer)), integerValue(integer), isInteger(true) {}
    void NumberExpr::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"NumberLiteral\",\"value\":";
        if (isInteger)
            out << integerValue;
        else
            out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        out << ",\"literalType\":\"" << (isInteger ? "int" : "double") << "\"";
        writeLine(out, line);
        out << '}';
    }
    BoolExpr::BoolExpr(bool v, int ln) : Expr(ln), value(v) {}
    void BoolExpr::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"BoolLiteral\",\"value\":" << (value ? "true" : "false");
        writeLine(out, line);
        out << '}';
    }
    VariableExpr::VariableExpr(std::string n, int ln) : Expr(ln), name(std::move(n)) {}
    void VariableExpr::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"Variable\",\"name\":\"" << jsonEscape(name) << "\"";
        writeLine(out, line);
        out << '}';
    }
    UnaryExpr::UnaryExpr(std::string o, Expr *e, int ln) : Expr(ln), op(std::move(o)), operand(e) {}
    void UnaryExpr::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"UnaryExpression\",\"operator\":\"" << jsonEscape(op) << "\",\"operand\":";
        operand->writeJson(out);
        writeLine(out, line);
        out << '}';
    }
    BinaryExpr::BinaryExpr(std::string o, Expr *l, Expr *r, int ln) : Expr(ln), op(std::move(o)), left(l), right(r) {}
    void BinaryExpr::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"BinaryExpression\",\"operator\":\"" << jsonEscape(op) << "\",\"left\":";
        left->writeJson(out);
        out << ",\"right\":";
        right->writeJson(out);
        writeLine(out, line);
        out << '}';
    }
    CallExpr::CallExpr(std::string n, std::vector<Expr *> *args, int ln) : Expr(ln), name(std::move(n))
    {
        if (args)
        {
            for (Expr *arg : *args)
                arguments.emplace_back(arg);
            delete args;
        }
    }
    void CallExpr::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"FunctionCall\",\"name\":\"" << jsonEscape(name) << "\",\"arguments\":";
        writeExprArray(out, arguments);
        writeLine(out, line);
        out << '}';
    }
    BlockStmt::BlockStmt(std::vector<Stmt *> *items, int ln) : Stmt(ln)
    {
        if (items)
        {
            for (Stmt *item : *items)
                statements.emplace_back(item);
            delete items;
        }
    }
    void BlockStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"Block\",\"statements\":[";
        for (std::size_t i = 0; i < statements.size(); ++i)
        {
            if (i)
                out << ',';
            statements[i]->writeJson(out);
        }
        out << ']';
        writeLine(out, line);
        out << '}';
    }
    VarDeclStmt::VarDeclStmt(Type t, std::string n, Expr *init, int ln) : Stmt(ln), type(t), name(std::move(n)), initializer(init) {}
    void VarDeclStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"VariableDeclaration\",\"declaredType\":\"" << typeName(type) << "\",\"name\":\"" << jsonEscape(name) << "\",\"initializer\":";
        if (initializer)
            initializer->writeJson(out);
        else
            out << "null";
        writeLine(out, line);
        out << '}';
    }
    AssignStmt::AssignStmt(std::string n, Expr *v, int ln) : Stmt(ln), name(std::move(n)), value(v) {}
    void AssignStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"Assignment\",\"name\":\"" << jsonEscape(name) << "\",\"value\":";
        value->writeJson(out);
        writeLine(out, line);
        out << '}';
    }
    IfStmt::IfStmt(Expr *c, Stmt *t, Stmt *e, int ln) : Stmt(ln), condition(c), thenBranch(t), elseBranch(e) {}
    void IfStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"IfStatement\",\"condition\":";
        condition->writeJson(out);
        out << ",\"thenBranch\":";
        thenBranch->writeJson(out);
        out << ",\"elseBranch\":";
        if (elseBranch)
            elseBranch->writeJson(out);
        else
            out << "null";
        writeLine(out, line);
        out << '}';
    }
    WhileStmt::WhileStmt(Expr *c, Stmt *b, int ln) : Stmt(ln), condition(c), body(b) {}
    void WhileStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"WhileStatement\",\"condition\":";
        condition->writeJson(out);
        out << ",\"body\":";
        body->writeJson(out);
        writeLine(out, line);
        out << '}';
    }
    ForStmt::ForStmt(Stmt *i, Expr *c, Stmt *s, Stmt *b, int ln) : Stmt(ln), initializer(i), condition(c), step(s), body(b) {}
    void ForStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"ForStatement\",\"initializer\":";
        if (initializer)
            initializer->writeJson(out);
        else
            out << "null";
        out << ",\"condition\":";
        if (condition)
            condition->writeJson(out);
        else
            out << "null";
        out << ",\"step\":";
        if (step)
            step->writeJson(out);
        else
            out << "null";
        out << ",\"body\":";
        body->writeJson(out);
        writeLine(out, line);
        out << '}';
    }
    ReturnStmt::ReturnStmt(Expr *v, int ln) : Stmt(ln), value(v) {}
    void ReturnStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"ReturnStatement\",\"value\":";
        if (value)
            value->writeJson(out);
        else
            out << "null";
        writeLine(out, line);
        out << '}';
    }
    ExprStmt::ExprStmt(Expr *e, int ln) : Stmt(ln), expression(e) {}
    void ExprStmt::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"ExpressionStatement\",\"expression\":";
        expression->writeJson(out);
        writeLine(out, line);
        out << '}';
    }
    FunctionDecl::FunctionDecl(Type rt, std::string n, std::vector<Parameter> *params, BlockStmt *b, int ln)
        : Node(ln), returnType(rt), name(std::move(n)), body(b)
    {
        if (params)
        {
            parameters = std::move(*params);
            delete params;
        }
    }
    void FunctionDecl::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"FunctionDeclaration\",\"name\":\"" << jsonEscape(name) << "\",\"returnType\":\"" << typeName(returnType) << "\",\"parameters\":[";
        for (std::size_t i = 0; i < parameters.size(); ++i)
        {
            if (i)
                out << ',';
            out << "{\"type\":\"Parameter\",\"name\":\"" << jsonEscape(parameters[i].name) << "\",\"dataType\":\"" << typeName(parameters[i].type) << "\",\"line\":" << parameters[i].line << '}';
        }
        out << "],\"body\":";
        body->writeJson(out);
        writeLine(out, line);
        out << '}';
    }
    Program::Program(std::vector<FunctionDecl *> *fs)
    {
        if (fs)
        {
            for (FunctionDecl *f : *fs)
                functions.emplace_back(f);
            delete fs;
        }
    }
    void Program::writeJson(std::ostream &out) const
    {
        out << "{\"type\":\"Program\",\"functions\":[";
        for (std::size_t i = 0; i < functions.size(); ++i)
        {
            if (i)
                out << ',';
            functions[i]->writeJson(out);
        }
        out << "]}";
    }
    std::string astToJson(const Program &program)
    {
        std::ostringstream out;
        program.writeJson(out);
        return out.str();
    }

} // namespace mini
