tinyc
=====

Here is a very simple toy compiler for a toy virtual machine.
It geatly helped me to see and understand what compiler is
(especially ast, vm and code generation).

It is just a rewrite of Marc Feeley's tinyc.c. Code of
which I read, tear apart, understand and as a result wrote this.

To run and test it:
$ gcc compiler.c
$ ./a.out < examples/1

Any comments are welcome!

Here is a grammar of the language:
<program> ::= <statement>
<statement> ::= "if" <paren_expr> <statement> |
                "if" <paren_expr> <statement> "else" <statement> |
                "while" <paren_expr> <statement> |
                "do" <statement> "while" <paren_expr> ";" |
                "{" { <statement> } "}" |
                <expr> ";" |
                ";"
<paren_expr> ::= "(" <expr> ")"
<expr> ::= <test> | <id> "=" <expr>
<test> ::= <sum> | <sum> "<" <sum>
<sum> ::= <term> | <sum> "+" <term> | <sum> "-" <term>
<term> ::= <id> | <int> | <paren_expr>
<id> ::= "a" | "b" | "c" | "d" | ... | "z"
<int> ::= <an_unsigned_decimal_integer>
