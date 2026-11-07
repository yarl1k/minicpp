# MiniCpp: короткий опис граматики

Нижче наведено читабельний EBNF-опис підмножини. Авторитетною реалізацією граматики є `src/parser.y`.

```ebnf
program          = { function } ;
function         = type, identifier, "(", [ parameters ], ")", block ;
parameters       = parameter, { ",", parameter } ;
parameter        = type, identifier ;
type             = "int" | "double" | "bool" | "void" ;

block            = "{", { statement }, "}" ;
statement        = block
                 | variable-declaration, ";"
                 | assignment, ";"
                 | if-statement
                 | while-statement
                 | for-statement
                 | "return", [ expression ], ";"
                 | expression, ";" ;

variable-declaration = type, identifier, [ "=", expression ] ;
assignment       = identifier, "=", expression
                 | identifier, "++"
                 | identifier, "--" ;
if-statement     = "if", "(", expression, ")", statement,
                   [ "else", statement ] ;
while-statement  = "while", "(", expression, ")", statement ;
for-statement    = "for", "(", [ for-initializer ], ";",
                   [ expression ], ";", [ for-step ], ")", statement ;
for-initializer  = variable-declaration | assignment ;
for-step         = assignment | expression ;

expression       = logical-or ;
logical-or       = logical-and, { "||", logical-and } ;
logical-and      = equality, { "&&", equality } ;
equality         = comparison, { ("==" | "!="), comparison } ;
comparison       = additive, { ("<" | ">" | "<=" | ">="), additive } ;
additive         = multiplicative, { ("+" | "-"), multiplicative } ;
multiplicative   = unary, { ("*" | "/" | "%"), unary } ;
unary            = ("-" | "!"), unary | primary ;
primary          = integer-literal | double-literal
                 | "true" | "false"
                 | identifier
                 | identifier, "(", [ arguments ], ")"
                 | "(", expression, ")" ;
arguments        = expression, { ",", expression } ;
```

## Пріоритет операторів

Від найнижчого до найвищого:

1. `||`
2. `&&`
3. `==`, `!=`
4. `<`, `>`, `<=`, `>=`
5. `+`, `-`
6. `*`, `/`, `%`
7. унарні `-`, `!`

Бінарні оператори лівоасоціативні. Порівняння в одній групі оголошені неасоціативними, тому ланцюжок на кшталт `a < b < c` відхиляється. Для `if/else` в Bison задано пріоритет, який прив'язує `else` до найближчого незакритого `if`.

## Семантичні правила підмножини

- Точка входу — функція `main()` без параметрів.
- Типи параметрів, аргументів, змінних і значень `return` мають збігатися точно.
- `void` дозволений лише як тип повернення функції.
- Умови `if`, `while` і `for` повинні мати тип `bool`.
- `%` приймає тільки `int`.
- `&&`, `||` і `!` приймають тільки `bool`.
- Non-void функція повинна мати гарантований `return`: безумовний `return` у блоці або `if/else`, у якому обидві гілки повертають значення.
- Функції можуть викликати одна одну незалежно від порядку оголошення; рекурсія підтримується.
