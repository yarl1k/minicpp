%{
#include "ast.hpp"
#include <cstdlib>
#include <string>
#include <vector>

mini::Program* g_program = nullptr;
std::vector<std::string> g_parse_errors;
%}

%code requires {
#include "ast.hpp"
#include <string>
#include <vector>
}

/* The scanner is generated separately by Flex.  Expose its entry point
   in the generated parser header so parser.tab.cpp can call it. */
%code provides {
int yylex(void);
}

%code {
static void destroyFunctions(std::vector<mini::FunctionDecl*>* items) {
    if (!items) return; for (auto* item : *items) delete item; delete items;
}
static void destroyStatements(std::vector<mini::Stmt*>* items) {
    if (!items) return; for (auto* item : *items) delete item; delete items;
}
static void destroyExpressions(std::vector<mini::Expr*>* items) {
    if (!items) return; for (auto* item : *items) delete item; delete items;
}

/* With the selected Bison C skeleton, yyerror is called with one argument.
   Flex maintains yylineno, which gives us a useful source line here. */
extern int yylineno;
void yyerror(const char* message) {
    const int line = yylineno > 0 ? yylineno : 1;
    g_parse_errors.push_back("line " + std::to_string(line) + ": " + message);
}
}

%define parse.error detailed
%define lr.type lalr
%locations

%union {
    long long integer;
    double real;
    char* text;
    mini::Type type;
    mini::Expr* expr;
    mini::Stmt* stmt;
    mini::BlockStmt* block;
    mini::FunctionDecl* function;
    mini::Program* program;
    mini::Parameter* parameter;
    std::vector<mini::FunctionDecl*>* functions;
    std::vector<mini::Stmt*>* statements;
    std::vector<mini::Expr*>* expressions;
    std::vector<mini::Parameter>* parameters;
}

%token KW_INT KW_DOUBLE KW_BOOL KW_VOID KW_IF KW_ELSE KW_WHILE KW_FOR KW_RETURN
%token KW_TRUE KW_FALSE
%token <integer> INT_LITERAL
%token <real> DOUBLE_LITERAL
%token <text> IDENTIFIER
%token EQ NE LE GE AND OR INC DEC INVALID

%type <type> type
%type <program> program
%type <functions> function_list
%type <function> function_definition
%type <parameters> parameter_list_opt parameter_list
%type <parameter> parameter
%type <block> block
%type <statements> statement_list
%type <stmt> statement variable_declaration assignment if_statement while_statement for_statement
%type <stmt> for_initializer_opt for_step_opt
%type <expr> expression expression_opt initializer_opt
%type <expressions> argument_list_opt argument_list

%destructor { free($$); } <text>
%destructor { delete $$; } <expr> <stmt> <block> <function> <program> <parameter>
%destructor { destroyFunctions($$); } <functions>
%destructor { destroyStatements($$); } <statements>
%destructor { destroyExpressions($$); } <expressions>
%destructor { delete $$; } <parameters>

%left OR
%left AND
%nonassoc EQ NE
%nonassoc '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right NOT UMINUS
%nonassoc LOWER_THAN_ELSE
%nonassoc KW_ELSE

%start program

%%

program:
    function_list
    {
        $$ = new mini::Program($1);
        g_program = $$;
        /* Transfer ownership to compileSource(); do not let Bison's
           %destructor free the Program after a successful parse. */
        $$ = nullptr;
    }
;

function_list:
    function_definition
    {
        $$ = new std::vector<mini::FunctionDecl*>();
        $$->push_back($1);
    }
  | function_list function_definition
    {
        $1->push_back($2);
        $$ = $1;
    }
;

function_definition:
    type IDENTIFIER '(' parameter_list_opt ')' block
    {
        $$ = new mini::FunctionDecl($1, $2, $4, $6, @1.first_line);
        free($2);
    }
;

type:
    KW_INT    { $$ = mini::Type::Int; }
  | KW_DOUBLE { $$ = mini::Type::Double; }
  | KW_BOOL   { $$ = mini::Type::Bool; }
  | KW_VOID   { $$ = mini::Type::Void; }
;

parameter_list_opt:
    %empty { $$ = new std::vector<mini::Parameter>(); }
  | parameter_list { $$ = $1; }
;

parameter_list:
    parameter
    {
        $$ = new std::vector<mini::Parameter>();
        $$->push_back(*$1);
        delete $1;
    }
  | parameter_list ',' parameter
    {
        $1->push_back(*$3);
        delete $3;
        $$ = $1;
    }
;

parameter:
    type IDENTIFIER
    {
        $$ = new mini::Parameter{$1, $2, @2.first_line};
        free($2);
    }
;

block:
    '{' statement_list '}'
    {
        $$ = new mini::BlockStmt($2, @1.first_line);
    }
;

statement_list:
    %empty { $$ = new std::vector<mini::Stmt*>(); }
  | statement_list statement
    {
        $1->push_back($2);
        $$ = $1;
    }
;

statement:
    variable_declaration ';' { $$ = $1; }
  | assignment ';' { $$ = $1; }
  | if_statement { $$ = $1; }
  | while_statement { $$ = $1; }
  | for_statement { $$ = $1; }
  | KW_RETURN expression_opt ';'
    { $$ = new mini::ReturnStmt($2, @1.first_line); }
  | expression ';'
    { $$ = new mini::ExprStmt($1, @1.first_line); }
  | block { $$ = $1; }
;

variable_declaration:
    type IDENTIFIER initializer_opt
    {
        $$ = new mini::VarDeclStmt($1, $2, $3, @1.first_line);
        free($2);
    }
;

initializer_opt:
    %empty { $$ = nullptr; }
  | '=' expression { $$ = $2; }
;

assignment:
    IDENTIFIER '=' expression
    {
        $$ = new mini::AssignStmt($1, $3, @1.first_line);
        free($1);
    }
  | IDENTIFIER INC
    {
        std::string name($1); free($1);
        auto* left = new mini::VariableExpr(name, @1.first_line);
        auto* one = new mini::NumberExpr(1.0, true, @2.first_line);
        $$ = new mini::AssignStmt(name, new mini::BinaryExpr("+", left, one, @2.first_line), @1.first_line);
    }
  | IDENTIFIER DEC
    {
        std::string name($1); free($1);
        auto* left = new mini::VariableExpr(name, @1.first_line);
        auto* one = new mini::NumberExpr(1.0, true, @2.first_line);
        $$ = new mini::AssignStmt(name, new mini::BinaryExpr("-", left, one, @2.first_line), @1.first_line);
    }
;

if_statement:
    KW_IF '(' expression ')' statement %prec LOWER_THAN_ELSE
    { $$ = new mini::IfStmt($3, $5, nullptr, @1.first_line); }
  | KW_IF '(' expression ')' statement KW_ELSE statement
    { $$ = new mini::IfStmt($3, $5, $7, @1.first_line); }
;

while_statement:
    KW_WHILE '(' expression ')' statement
    { $$ = new mini::WhileStmt($3, $5, @1.first_line); }
;

for_statement:
    KW_FOR '(' for_initializer_opt ';' expression_opt ';' for_step_opt ')' statement
    { $$ = new mini::ForStmt($3, $5, $7, $9, @1.first_line); }
;

for_initializer_opt:
    %empty { $$ = nullptr; }
  | variable_declaration { $$ = $1; }
  | assignment { $$ = $1; }
;

for_step_opt:
    %empty { $$ = nullptr; }
  | assignment { $$ = $1; }
  | expression { $$ = new mini::ExprStmt($1, @1.first_line); }
;

expression_opt:
    %empty { $$ = nullptr; }
  | expression { $$ = $1; }
;

expression:
    INT_LITERAL
    { $$ = new mini::NumberExpr(static_cast<double>($1), true, @1.first_line); }
  | DOUBLE_LITERAL
    { $$ = new mini::NumberExpr($1, false, @1.first_line); }
  | KW_TRUE { $$ = new mini::BoolExpr(true, @1.first_line); }
  | KW_FALSE { $$ = new mini::BoolExpr(false, @1.first_line); }
  | IDENTIFIER
    { $$ = new mini::VariableExpr($1, @1.first_line); free($1); }
  | IDENTIFIER '(' argument_list_opt ')'
    { $$ = new mini::CallExpr($1, $3, @1.first_line); free($1); }
  | '(' expression ')' { $$ = $2; }
  | '-' expression %prec UMINUS
    { $$ = new mini::UnaryExpr("-", $2, @1.first_line); }
  | '!' expression %prec NOT
    { $$ = new mini::UnaryExpr("!", $2, @1.first_line); }
  | expression '+' expression
    { $$ = new mini::BinaryExpr("+", $1, $3, @2.first_line); }
  | expression '-' expression
    { $$ = new mini::BinaryExpr("-", $1, $3, @2.first_line); }
  | expression '*' expression
    { $$ = new mini::BinaryExpr("*", $1, $3, @2.first_line); }
  | expression '/' expression
    { $$ = new mini::BinaryExpr("/", $1, $3, @2.first_line); }
  | expression '%' expression
    { $$ = new mini::BinaryExpr("%", $1, $3, @2.first_line); }
  | expression EQ expression
    { $$ = new mini::BinaryExpr("==", $1, $3, @2.first_line); }
  | expression NE expression
    { $$ = new mini::BinaryExpr("!=", $1, $3, @2.first_line); }
  | expression '<' expression
    { $$ = new mini::BinaryExpr("<", $1, $3, @2.first_line); }
  | expression '>' expression
    { $$ = new mini::BinaryExpr(">", $1, $3, @2.first_line); }
  | expression LE expression
    { $$ = new mini::BinaryExpr("<=", $1, $3, @2.first_line); }
  | expression GE expression
    { $$ = new mini::BinaryExpr(">=", $1, $3, @2.first_line); }
  | expression AND expression
    { $$ = new mini::BinaryExpr("&&", $1, $3, @2.first_line); }
  | expression OR expression
    { $$ = new mini::BinaryExpr("||", $1, $3, @2.first_line); }
;

argument_list_opt:
    %empty { $$ = new std::vector<mini::Expr*>(); }
  | argument_list { $$ = $1; }
;

argument_list:
    expression
    {
        $$ = new std::vector<mini::Expr*>();
        $$->push_back($1);
    }
  | argument_list ',' expression
    {
        $1->push_back($3);
        $$ = $1;
    }
;

%%
