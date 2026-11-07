#include "fallback_parser.hpp"

#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace mini {
namespace {
enum class Kind {
    End, Invalid, Identifier, Integer, Real,
    Int, Double, Bool, Void, If, Else, While, For, Return, True, False,
    Eq, Ne, Le, Ge, And, Or, Inc, Dec,
    Plus, Minus, Star, Slash, Percent, Assign, Less, Greater, Not,
    LParen, RParen, LBrace, RBrace, Comma, Semicolon
};
struct Token { Kind kind; std::string text; double number = 0; long long integer = 0; int line = 1; };

class Lexer {
public:
    explicit Lexer(const std::string& source) : source_(source) {}
    Token next() {
        skipTrivia();
        const int startLine = line_;
        if (unterminatedComment_) { unterminatedComment_ = false; return {Kind::Invalid, "unterminated block comment", 0, 0, unterminatedLine_}; }
        if (pos_ >= source_.size()) return {Kind::End, "", 0, 0, startLine};
        char c = source_[pos_];
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = pos_++;
            while (pos_ < source_.size()) {
                char next = source_[pos_];
                if (!std::isalnum(static_cast<unsigned char>(next)) && next != '_') break;
                ++pos_;
            }
            std::string text = source_.substr(start, pos_ - start);
            static const std::unordered_map<std::string, Kind> keywords = {
                {"int",Kind::Int},{"double",Kind::Double},{"bool",Kind::Bool},{"void",Kind::Void},
                {"if",Kind::If},{"else",Kind::Else},{"while",Kind::While},{"for",Kind::For},
                {"return",Kind::Return},{"true",Kind::True},{"false",Kind::False}
            };
            auto found = keywords.find(text);
            return {found == keywords.end() ? Kind::Identifier : found->second, text, 0, 0, startLine};
        }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && pos_ + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_ + 1])))) {
            std::size_t start = pos_;
            bool real = false;
            if (source_[pos_] != '.') {
                while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) ++pos_;
            }
            if (pos_ < source_.size() && source_[pos_] == '.') {
                real = true; ++pos_;
                while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) ++pos_;
            }
            if (pos_ < source_.size() && (source_[pos_] == 'e' || source_[pos_] == 'E')) {
                real = true; ++pos_;
                if (pos_ < source_.size() && (source_[pos_] == '+' || source_[pos_] == '-')) ++pos_;
                std::size_t exponentStart = pos_;
                while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) ++pos_;
                if (exponentStart == pos_) return {Kind::Invalid, source_.substr(start, pos_ - start), 0, 0, startLine};
            }
            std::string text = source_.substr(start, pos_ - start);
            try {
                return real ? Token{Kind::Real,text,std::stod(text),0,startLine}
                            : Token{Kind::Integer,text,0,std::stoll(text),startLine};
            } catch (...) { return {Kind::Invalid,text,0,0,startLine}; }
        }
        auto match = [&](const char* text, Kind kind) -> bool {
            std::size_t length = std::char_traits<char>::length(text);
            if (source_.compare(pos_, length, text) != 0) return false;
            pos_ += length; current_ = {kind, text, 0, 0, startLine}; return true;
        };
        if (match("==",Kind::Eq) || match("!=",Kind::Ne) || match("<=",Kind::Le) ||
            match(">=",Kind::Ge) || match("&&",Kind::And) || match("||",Kind::Or) ||
            match("++",Kind::Inc) || match("--",Kind::Dec)) return current_;
        ++pos_;
        switch (c) {
            case '+': return {Kind::Plus,"+",0,0,startLine}; case '-': return {Kind::Minus,"-",0,0,startLine};
            case '*': return {Kind::Star,"*",0,0,startLine}; case '/': return {Kind::Slash,"/",0,0,startLine};
            case '%': return {Kind::Percent,"%",0,0,startLine}; case '=': return {Kind::Assign,"=",0,0,startLine};
            case '<': return {Kind::Less,"<",0,0,startLine}; case '>': return {Kind::Greater,">",0,0,startLine};
            case '!': return {Kind::Not,"!",0,0,startLine}; case '(': return {Kind::LParen,"(",0,0,startLine};
            case ')': return {Kind::RParen,")",0,0,startLine}; case '{': return {Kind::LBrace,"{",0,0,startLine};
            case '}': return {Kind::RBrace,"}",0,0,startLine}; case ',': return {Kind::Comma,",",0,0,startLine};
            case ';': return {Kind::Semicolon,";",0,0,startLine}; default: return {Kind::Invalid,std::string(1,c),0,0,startLine};
        }
    }
private:
    const std::string& source_; std::size_t pos_ = 0; int line_ = 1; Token current_{};
    bool unterminatedComment_ = false; int unterminatedLine_ = 1;
    void skipTrivia() {
        while (pos_ < source_.size()) {
            char c = source_[pos_];
            if (c == ' ' || c == '\t' || c == '\r') { ++pos_; continue; }
            if (c == '\n') { ++line_; ++pos_; continue; }
            if (source_.compare(pos_,2,"//") == 0) {
                pos_ += 2; while (pos_ < source_.size() && source_[pos_] != '\n') ++pos_; continue;
            }
            if (source_.compare(pos_,2,"/*") == 0) {
                int commentLine = line_;
                pos_ += 2; bool closed = false;
                while (pos_ < source_.size()) {
                    if (source_[pos_] == '\n') ++line_;
                    if (source_.compare(pos_,2,"*/") == 0) { pos_ += 2; closed = true; break; }
                    ++pos_;
                }
                if (!closed) { unterminatedComment_ = true; unterminatedLine_ = commentLine; return; }
                continue;
            }
            break;
        }
    }
};

class Parser {
public:
    explicit Parser(const std::string& source) : lexer_(source) { advance(); }
    std::unique_ptr<Program> parse() {
        auto* functions = new std::vector<FunctionDecl*>();
        while (current_.kind != Kind::End) functions->push_back(parseFunction());
        if (functions->empty()) fail("program must contain at least one function");
        return std::make_unique<Program>(functions);
    }
private:
    Lexer lexer_; Token current_;
    void advance() { current_ = lexer_.next(); }
    bool accept(Kind kind) { if (current_.kind != kind) return false; advance(); return true; }
    Token expect(Kind kind, const std::string& what) {
        if (current_.kind != kind) fail("expected " + what + ", got '" + current_.text + "'");
        Token result = current_; advance(); return result;
    }
    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("line " + std::to_string(current_.line) + ": " + message);
    }
    bool isType() const { return current_.kind == Kind::Int || current_.kind == Kind::Double || current_.kind == Kind::Bool || current_.kind == Kind::Void; }
    Type parseType() {
        Token token = current_; advance();
        switch (token.kind) { case Kind::Int:return Type::Int; case Kind::Double:return Type::Double; case Kind::Bool:return Type::Bool; case Kind::Void:return Type::Void; default: fail("expected a type"); }
    }
    FunctionDecl* parseFunction() {
        int line = current_.line; Type type = parseType();
        std::string name = expect(Kind::Identifier,"function name").text;
        expect(Kind::LParen,"'('");
        auto* parameters = new std::vector<Parameter>();
        if (current_.kind != Kind::RParen) {
            do {
                int pLine = current_.line; Type pType = parseType();
                std::string pName = expect(Kind::Identifier,"parameter name").text;
                parameters->push_back({pType,pName,pLine});
            } while (accept(Kind::Comma));
        }
        expect(Kind::RParen,"')'");
        auto* body = parseBlock();
        return new FunctionDecl(type,name,parameters,body,line);
    }
    BlockStmt* parseBlock() {
        int line = expect(Kind::LBrace,"'{'").line;
        auto* statements = new std::vector<Stmt*>();
        while (current_.kind != Kind::RBrace && current_.kind != Kind::End) statements->push_back(parseStatement());
        expect(Kind::RBrace,"'}'");
        return new BlockStmt(statements,line);
    }
    Stmt* parseVariableDeclaration() {
        int line = current_.line; Type type = parseType();
        std::string name = expect(Kind::Identifier,"variable name").text;
        Expr* initializer = nullptr;
        if (accept(Kind::Assign)) initializer = parseExpression();
        return new VarDeclStmt(type,name,initializer,line);
    }
    AssignStmt* parseAssignment() {
        int line = current_.line; std::string name = expect(Kind::Identifier,"variable name").text;
        if (accept(Kind::Inc)) {
            return new AssignStmt(name, new BinaryExpr("+", new VariableExpr(name,line), new NumberExpr(1.0,true,line), line), line);
        }
        if (accept(Kind::Dec)) {
            return new AssignStmt(name, new BinaryExpr("-", new VariableExpr(name,line), new NumberExpr(1.0,true,line), line), line);
        }
        expect(Kind::Assign,"'='");
        return new AssignStmt(name,parseExpression(),line);
    }
    Stmt* parseStatement() {
        if (current_.kind == Kind::LBrace) return parseBlock();
        if (isType()) { Stmt* result = parseVariableDeclaration(); expect(Kind::Semicolon,"';'"); return result; }
        if (current_.kind == Kind::If) {
            int line = current_.line; advance(); expect(Kind::LParen,"'('"); Expr* condition = parseExpression(); expect(Kind::RParen,"')'");
            Stmt* thenBranch = parseStatement(); Stmt* elseBranch = accept(Kind::Else) ? parseStatement() : nullptr;
            return new IfStmt(condition,thenBranch,elseBranch,line);
        }
        if (current_.kind == Kind::While) {
            int line = current_.line; advance(); expect(Kind::LParen,"'('"); Expr* condition = parseExpression(); expect(Kind::RParen,"')'");
            return new WhileStmt(condition,parseStatement(),line);
        }
        if (current_.kind == Kind::For) {
            int line = current_.line; advance(); expect(Kind::LParen,"'('");
            Stmt* initializer = nullptr;
            if (current_.kind != Kind::Semicolon) initializer = isType() ? parseVariableDeclaration() : parseAssignment();
            expect(Kind::Semicolon,"';'");
            Expr* condition = current_.kind == Kind::Semicolon ? nullptr : parseExpression();
            expect(Kind::Semicolon,"';'");
            Stmt* step = nullptr;
            if (current_.kind != Kind::RParen) {
                if (current_.kind == Kind::Identifier && nextIsAssign()) step = parseAssignment();
                else step = new ExprStmt(parseExpression(),line);
            }
            expect(Kind::RParen,"')'");
            return new ForStmt(initializer,condition,step,parseStatement(),line);
        }
        if (current_.kind == Kind::Return) {
            int line = current_.line; advance(); Expr* value = current_.kind == Kind::Semicolon ? nullptr : parseExpression();
            expect(Kind::Semicolon,"';'"); return new ReturnStmt(value,line);
        }
        if (current_.kind == Kind::Identifier && nextIsAssign()) {
            Stmt* result = parseAssignment(); expect(Kind::Semicolon,"';'"); return result;
        }
        int line = current_.line; Expr* expression = parseExpression();
        expect(Kind::Semicolon,"';'"); return new ExprStmt(expression,line);
    }
    bool nextIsAssign() const {
        // The lexer is deliberately single-token lookahead; save/restore is unnecessary for
        // the common identifier '=' statement because we peek into the source-independent token stream.
        Token saved = current_;
        (void)saved;
        // A minimal token lookahead is supplied by a copy of the lexer state in peekAssign().
        return peekAssign();
    }
    bool peekAssign() const {
        Lexer copy = lexer_;
        Kind next = copy.next().kind;
        return next == Kind::Assign || next == Kind::Inc || next == Kind::Dec;
    }
    static int precedence(Kind kind) {
        switch (kind) {
            case Kind::Or:return 1; case Kind::And:return 2;
            case Kind::Eq:case Kind::Ne:return 3;
            case Kind::Less:case Kind::Greater:case Kind::Le:case Kind::Ge:return 4;
            case Kind::Plus:case Kind::Minus:return 5;
            case Kind::Star:case Kind::Slash:case Kind::Percent:return 6;
            default:return 0;
        }
    }
    std::string operatorText(Kind kind) const {
        switch(kind) { case Kind::Or:return "||"; case Kind::And:return "&&"; case Kind::Eq:return "=="; case Kind::Ne:return "!=";
            case Kind::Less:return "<"; case Kind::Greater:return ">"; case Kind::Le:return "<="; case Kind::Ge:return ">=";
            case Kind::Plus:return "+"; case Kind::Minus:return "-"; case Kind::Star:return "*"; case Kind::Slash:return "/"; case Kind::Percent:return "%"; default:return "?"; }
    }
    Expr* parseExpression(int minPrecedence = 1) {
        Expr* left = parseUnary();
        while (true) {
            int prec = precedence(current_.kind);
            if (prec < minPrecedence) break;
            Token op = current_; advance();
            Expr* right = parseExpression(prec + 1);
            left = new BinaryExpr(operatorText(op.kind),left,right,op.line);
        }
        return left;
    }
    Expr* parseUnary() {
        if (current_.kind == Kind::Minus || current_.kind == Kind::Not) {
            Token op = current_; advance();
            return new UnaryExpr(op.kind == Kind::Minus ? "-" : "!",parseUnary(),op.line);
        }
        return parsePrimary();
    }
    Expr* parsePrimary() {
        Token token = current_; advance();
        if (token.kind == Kind::Integer) return new NumberExpr(static_cast<double>(token.integer),true,token.line);
        if (token.kind == Kind::Real) return new NumberExpr(token.number,false,token.line);
        if (token.kind == Kind::True) return new BoolExpr(true,token.line);
        if (token.kind == Kind::False) return new BoolExpr(false,token.line);
        if (token.kind == Kind::Identifier) {
            if (!accept(Kind::LParen)) return new VariableExpr(token.text,token.line);
            auto* args = new std::vector<Expr*>();
            if (current_.kind != Kind::RParen) {
                do { args->push_back(parseExpression()); } while (accept(Kind::Comma));
            }
            expect(Kind::RParen,"')'");
            return new CallExpr(token.text,args,token.line);
        }
        if (token.kind == Kind::LParen) {
            Expr* expression = parseExpression(); expect(Kind::RParen,"')'"); return expression;
        }
        fail("expected an expression, got '" + token.text + "'");
    }
};
}

FallbackParseOutput parseWithFallbackParser(const std::string& source) {
    FallbackParseOutput output;
    try { output.program = Parser(source).parse(); }
    catch (const std::exception& error) { output.errors.push_back(error.what()); }
    return output;
}

} // namespace mini
