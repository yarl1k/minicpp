%option noyywrap yylineno nounput noinput
%{
#include "parser.tab.h"
#include <cstdlib>
#include <cstring>
#define YY_USER_ACTION do { yylloc.first_line = yylloc.last_line = yylineno; yylloc.first_column = 1; yylloc.last_column = 1; } while (0);
%}

DIGIT       [0-9]
IDENT       [a-zA-Z_][a-zA-Z0-9_]*

%%
[ \t\r\n]+                 ;
"//"[^\n]*                 ;
"/*"([^*]|\*+[^*/])*\*+"/"  ;

"int"                       return KW_INT;
"double"                    return KW_DOUBLE;
"bool"                      return KW_BOOL;
"void"                      return KW_VOID;
"if"                        return KW_IF;
"else"                      return KW_ELSE;
"while"                     return KW_WHILE;
"for"                       return KW_FOR;
"return"                    return KW_RETURN;
"true"                      return KW_TRUE;
"false"                     return KW_FALSE;

"=="                        return EQ;
"!="                        return NE;
"<="                        return LE;
">="                        return GE;
"&&"                        return AND;
"||"                        return OR;
"++"                        return INC;
"--"                        return DEC;

(({DIGIT}+\.{DIGIT}*)|(\.{DIGIT}+))([eE][+-]?{DIGIT}+)?  { yylval.real = std::strtod(yytext, nullptr); return DOUBLE_LITERAL; }
{DIGIT}+[eE][+-]?{DIGIT}+                { yylval.real = std::strtod(yytext, nullptr); return DOUBLE_LITERAL; }
{DIGIT}+                                { yylval.integer = std::strtoll(yytext, nullptr, 10); return INT_LITERAL; }
{IDENT}                                 { yylval.text = ::strdup(yytext); return IDENTIFIER; }

[+*/%(){};,<>!=\-]          return yytext[0];
.                            return INVALID;
%%
