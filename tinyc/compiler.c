/*
 * 2014-Jun-25 by Nikolay Vasylchyshyn, <younghead@ukr.net>
 * 
 * Here is a very simple toy compiler for a toy virtual machine.
 * It helped me greatly to see and understand what compiler is
 * (especially ast, vm and code generation).
 *
 * It is just a rewrite of Marc Feeley's tinyc.c. Code of
 * which I read, tear apart, understand and as a result wrote this.
 *
 * To run and test it:
 * $ gcc compiler.c
 * $ ./a.out < examples/1
 *
 * Any comments are welcome!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 *  <program> ::= <statement>
 *  <statement> ::= "if" <paren_expr> <statement> |
 *                  "if" <paren_expr> <statement> "else" <statement> |
 *                  "while" <paren_expr> <statement> |
 *                  "do" <statement> "while" <paren_expr> ";" |
 *                  "{" { <statement> } "}" |
 *                  <expr> ";" |
 *                  ";"
 *  <paren_expr> ::= "(" <expr> ")"
 *  <expr> ::= <test> | <id> "=" <expr>
 *  <test> ::= <sum> | <sum> "<" <sum>
 *  <sum> ::= <term> | <sum> "+" <term> | <sum> "-" <term>
 *  <term> ::= <id> | <int> | <paren_expr>
 *  <id> ::= "a" | "b" | "c" | "d" | ... | "z"
 *  <int> ::= <an_unsigned_decimal_integer>
 */

/* LEXER **********************************************************************/
int lexem;
int value;

enum { L_DO, L_ELSE, L_IF, L_WHILE, L_ID, L_CONSTANT, L_LESS, L_EQ, L_SEMICOLON, L_LPAR, L_RPAR, L_LBRA, L_RBRA, L_PLUS, L_MINUS, L_END };
void get_lexem(void)
{
	int ch = getchar();

	while (isspace(ch))
		ch = getchar();

	switch (ch) {
		case EOF: lexem = L_END;       return;
		case '-': lexem = L_MINUS;     return;
		case '+': lexem = L_PLUS;      return;
		case '(': lexem = L_LPAR;      return;
		case ')': lexem = L_RPAR;      return;
		case '{': lexem = L_LBRA;      return;
		case '}': lexem = L_RBRA;      return;
		case '<': lexem = L_LESS;      return;
		case ';': lexem = L_SEMICOLON; return;
		case '=': lexem = L_EQ;        return;
	}

	if (isdigit(ch)) {
		value = 0;
		do {
			value = value * 10 + (ch-'0');
			ch = getchar();
		} while (isdigit(ch));
		ungetc(ch, stdin);
		lexem = L_CONSTANT;
		return;
	}

	if (isalpha(ch)) {
		char buffer[100]; /* FIXME: BUFFER OVERFLOW MAY BE HERE */
		char *bp = buffer;
		do {
			if (bp-buffer > sizeof(buffer)) {
				fprintf(stderr, "ERROR: too long identifier in the input.");
				abort();
			}
			*bp++ = ch;
			ch = getchar();
		} while (isalpha(ch));
		*bp = '\0';
		ungetc(ch, stdin);
		if (!strcmp(buffer, "do")) {
			lexem = L_DO;
			return;
		} else if (!strcmp(buffer, "else")) {
			lexem = L_ELSE;
			return;
		} else if (!strcmp(buffer, "if")) {
			lexem = L_IF;
			return;
		} else if (!strcmp(buffer, "while")) {
			lexem = L_WHILE;
			return;
		} else {
			if (strlen(buffer) > 1 || !islower(buffer[0])) {
				fprintf(stderr, "ERROR: only single lowercase letters are allowed as ID.\n");
				abort();
			}
			value = buffer[0] - 'a';
			lexem = L_ID;
			return;
		}
	}

	fprintf(stderr, "ERROR: get_lexem(): unexpected character '%c' in input stream.\n", ch);
	abort();

}

/* PARSER *********************************************************************/
struct node
{
	int type;
	int value;
	struct node *n1;
	struct node *n2;
	struct node *n3;
	/* value and child nodes can be in the union */
};

struct node *
create_node(int type)
{
	struct node *n = malloc(sizeof(struct node));
	if (n == NULL) {
		fprintf(stderr, "ERROR: not enough free memory.\n");
		abort();
	}
	memset(n, 0, sizeof(*n));
	n->type = type;
	return n;
}

enum { P_PROG, P_IF1, P_IF2, P_WHILE, P_DO, P_SET, P_LT, P_SUB, P_ADD, P_CONST, P_VAR, P_EXPR, P_SEQ, P_EMPTY };

struct node *paren_expr(void);

/*  <term> ::= <id> | <int> | <paren_expr> */
struct node *
term(void)
{
	struct node *n;
	if (lexem == L_ID) {
		n = create_node(P_VAR);
		n->value = value;
		get_lexem();
	} else if (lexem == L_CONSTANT) {
		n = create_node(P_CONST);
		n->value = value;
		get_lexem();
	} else if (lexem == L_LPAR) {
		n = paren_expr();
	} else {
		fprintf(stderr, "PARSER ERROR: term() expects ID, CONSTANT or (expr), but current lexem=%d\n", lexem);
		abort();
	}
	return n;
}

/*  <sum> ::= <term> | <sum> "+" <term> | <sum> "-" <term> */
struct node *
sum(void)
{
	struct node *n = term();
	/*for (get_lexem(); lexem == L_PLUS || lexem == L_MINUS; get_lexem()) {*/
	while (lexem == L_PLUS || lexem == L_MINUS) {
		struct node *t = create_node(lexem == L_PLUS ? P_ADD : P_SUB);
		t->n1 = n;
		get_lexem(); /* skip L_PLUS or L_MINUS */
		t->n2 = term();
		n = t;
	}
	return n;
}

/*  <test> ::= <sum> | <sum> "<" <sum> */
struct node *
test(void)
{
	struct node *n = sum();
	/*get_lexem();*/
	if (lexem == L_LESS) {
		struct node *x = create_node(P_LT);
		x->n1 = n;
		get_lexem();
		x->n2 = sum();
		n = x;
	}
	return n;
}

/*  <expr> ::= <test> | <id> "=" <expr> */
struct node *
expr(void)
{
	struct node *n = test();
	if (n->type == P_VAR && lexem == L_EQ) {
		struct node *t = create_node(P_SET);
		t->n1 = n;
		get_lexem(); /* skip L_EQ */
		t->n2 = expr();
		n = t;
	}
	return n;
}

/*  <paren_expr> ::= "(" <expr> ")" */
struct node *
paren_expr(void)
{
	struct node *n;
	if (lexem != L_LPAR) {
		fprintf(stderr, "PARSER ERROR: paren_expr(): expected '('\n");
		abort();
	}
	get_lexem();
	n = expr();
	if (lexem != L_RPAR) {
		fprintf(stderr, "PARSER ERROR: paren_expr(): expected ')'\n");
		abort();
	}
	get_lexem();
	return n;
}

/*  <statement> ::= "if" <paren_expr> <statement> |
 *                  "if" <paren_expr> <statement> "else" <statement> |
 *                  "while" <paren_expr> <statement> |
 *                  "do" <statement> "while" <paren_expr> ";" |
 *                  "{" { <statement> } "}" |
 *                  <expr> ";" |
 *                  ";"
 */
struct node *
statement(void)
{
	struct node *n;
	if (lexem == L_IF) {
		n = create_node(P_IF1);
		get_lexem();
		n->n1 = paren_expr();
		/*get_lexem();*/
		n->n2 = statement();
		/*get_lexem();*/
		if (lexem == L_ELSE) {
			n->type = P_IF2;
			get_lexem();
			n->n3 = statement();
		}
	} else if (lexem == L_WHILE) {
		n = create_node(P_WHILE);
		get_lexem();
		n->n1 = paren_expr();
		/*get_lexem();*/
		n->n2 = statement();
	} else if (lexem == L_DO) {
		n = create_node(P_DO);
		get_lexem();
		n->n1 = statement();
		/*get_lexem();*/
		if (lexem != L_WHILE) {
			fprintf(stderr, "PARSER ERROR: statement(): expected L_WHILE after L_DO\n");
			abort();
		}
		get_lexem();
		n->n2 = paren_expr();
		/*get_lexem();*/
		if (lexem != L_SEMICOLON) {
			fprintf(stderr, "PARSER ERROR: statement(): expected L_SEMICOLON at the and of L_DO\n");
			abort();
		}
		get_lexem();
	} else if (lexem == L_LBRA) {
		struct node *t;
		/*for (get_lexem(), n = NULL; lexem != L_RBRA; get_lexem()) {*/
		for (get_lexem(), n = NULL; lexem != L_RBRA; ) {
			t = n;
			n = create_node(P_SEQ);
			n->n1 = t;
			n->n2 = statement();
		}
		get_lexem(); /* skip L_RBRA */
	} else if (lexem == L_SEMICOLON) {
		n = create_node(P_EMPTY);
		get_lexem();
	} else { /* <expr> ";" */
		n = create_node(P_EXPR);
		n->n1 = expr();
		if (lexem != L_SEMICOLON) {
			fprintf(stderr, "PARSER ERROR: statement(): expected L_SEMICOLON at the and of expr (lexem=%d)\n",
					lexem);
			abort();
		}
		get_lexem();
	}
	return n;
}

/*  <program> ::= <statement> */
struct node *
program(void)
{
	struct node *n = create_node(P_PROG);
	get_lexem();
	n->n1 = statement();
	/*get_lexem();*/
	if (lexem != L_END) {
		fprintf(stderr, "PARSER ERROR: program(): expected EOF\n");
		abort();
	}
	return n;
}

/* VIRTUAL MACHINE ************************************************************/
enum { IFETCH, ISTORE, IPUSH, IPOP, IADD, ISUB, ILT, JZ, JNZ, JMP, HALT };
const char *instruction_names[] = { "IFETCH", "ISTORE", "IPUSH", "IPOP",
	"IADD", "ISUB", "ILT", "JZ", "JNZ", "JMP", "HALT" };
typedef signed char byte;
byte memory[1024];
int globals[26];
void
run()
{
	byte *ip = memory; /* Instruction Pointer */
	byte *sp = memory+sizeof(memory); /* Stack Pointer */
	/* SP always points to the top of the stack, so no -1 - it point just right after memory */
	/* sp always points to the data, not the empty cell */

	/*
	 * when cpu reads instruction ip points to it argument.
	 * The arg of JMP is how many bytes to skip after the
	 * JMP instruction itself, not after JMP arg!
	 */

again:
	switch (*ip++) {
		case IFETCH:
			*--sp = globals[*ip++];
			goto again;
		case ISTORE:
			globals[*ip++] = *sp;
			goto again;
		case IPUSH:
			*--sp = *ip++;
			goto again;
		case IPOP:
			sp++;
			goto again;
		case IADD:
			sp[1] = sp[1] + sp[0];
			sp++;
			goto again;
		case ISUB:
			sp[1] = sp[1] - sp[0];
			sp++;
			goto again;
		case ILT:
			sp[1] = sp[1] < sp[0];
			sp++;
			goto again;
		case JZ:
			if (*sp == 0)
				ip += *ip;
			else
				ip++;
			goto again;
		case JNZ:
			if (*sp)
				ip += *ip;
			else
				ip++;
			goto again;
		case JMP:
			ip += *ip;
			goto again;
		case HALT:
			break;
		default:
			fprintf(stderr, "VIRTUAL MACHINE ERROR: Unknown instruction: %d\n", *ip);
			abort();
	}
}

/* CODE GENERATOR *************************************************************/

byte *p = memory;
void generate(struct node *n)
{
	/* FIXME: no check for memory overflow */
	byte *q, *r;
	switch (n->type) {
		case P_PROG:
			generate(n->n1);
			*p++ = HALT;
			break;
		case P_IF1:
			generate(n->n1);
			*p++ = JZ;
			q = p++;
			generate(n->n2);
			*q = p-q;
			break;
		case P_IF2:
			generate(n->n1);
			*p++ = JZ;
			q = p++;
			generate(n->n2);
			*p++ = JMP;
			r = p++;
			*q = p-q;
			generate(n->n3);
			*r = p-r;
			break;
		case P_WHILE:
			q = p; /* loop start */
			generate(n->n1);
			*p++ = JZ;
			r = p++;
			generate(n->n2);
			*p++ = JMP;
			*p = q-p;
			p++;
			*r = p-r;
			break;
		case P_DO:
			q = p;
			generate(n->n1);
			generate(n->n2);
			*p++ = JNZ;
			*p = q-p;
			p++;
			break;
		case P_SET:
			generate(n->n2);
			*p++ = ISTORE;
			*p++ = n->n1->value;
			break;
		case P_LT:
			generate(n->n1);
			generate(n->n2);
			*p++ = ILT;
			break;
		case P_SUB:
			generate(n->n1);
			generate(n->n2);
			*p++ = ISUB;
			break;
		case P_ADD:
			generate(n->n1);
			generate(n->n2);
			*p++ = IADD;
			break;
		case P_CONST:
			*p++ = IPUSH;
			*p++ = n->value;
			break;
		case P_VAR:
			*p++ = IFETCH;
			*p++ = n->value;
			break;
		case P_EXPR:
			generate(n->n1);
			*p++ = IPOP;
			break;
		case P_SEQ:
			if (n->n1)
				generate(n->n1);
			generate(n->n2);
			break;
		case P_EMPTY:
			break;
		}
}

void dump_program()
{
#define COLORED(str) printf("\033[1m" str "\033[0m")
	byte *p = memory;
again:
	switch (*p++) {
		case IFETCH:
			COLORED(" IFETCH");
			printf(" %d", *p++);
			goto again;
		case ISTORE:
			COLORED(" ISTORE");
			printf(" %d", *p++);
			goto again;
		case IPUSH:
			COLORED(" IPUSH");
			printf(" %d", *p++);
			goto again;
		case JZ:
			COLORED(" JZ");
			printf(" %d", *p++);
			goto again;
		case JNZ:
			COLORED(" JNZ");
			printf(" %d", *p++);
			goto again;
		case JMP:
			COLORED(" JMP");
			printf(" %d", *p++);
			goto again;
		case HALT:
			COLORED(" HALT");
			break;
		default:
			printf("\033[1m %s\033[0m", instruction_names[p[-1]]);
			goto again;
	}
	printf("\n");
}

/* MISC ***********************************************************************/
void test_lexer(void)
{
	for (get_lexem(); lexem != L_END; get_lexem()) {
		switch (lexem) {
			case L_DO:          printf("DO\n");            break;
			case L_ELSE:        printf("ELSE\n");          break;
			case L_IF:          printf("IF\n");            break;
			case L_WHILE:       printf("WHILE\n");         break;
			case L_ID:          printf("%c\n", value+'a'); break;
			case L_CONSTANT:    printf("%d\n", value);     break;
			case L_LESS:        printf("<\n");             break;
			case L_EQ:          printf("=\n");             break;
			case L_SEMICOLON:   printf(";\n");             break;
			case L_LPAR:        printf("(\n");             break;
			case L_RPAR:        printf(")\n");             break;
			case L_LBRA:        printf("{\n");             break;
			case L_RBRA:        printf("}\n");             break;
			case L_PLUS:        printf("+\n");             break;
			case L_MINUS:       printf("-\n");             break;
		}
	}
}

int interpret_node(struct node *n)
{
	switch (n->type) {
		case P_PROG:
			interpret_node(n->n1);
			return 0;
		case P_IF1:
			if (interpret_node(n->n1))
				interpret_node(n->n2);
			return 0;
		case P_IF2:
			if (interpret_node(n->n1))
				interpret_node(n->n2);
			else
				interpret_node(n->n3);
			return 0;
		case P_WHILE:
			while (interpret_node(n->n1))
				interpret_node(n->n2);
			return 0;
		case P_DO:
			do {
				interpret_node(n->n1);
			} while (interpret_node(n->n2));
			return 0;
		case P_SET:
			return globals[n->n1->value] = interpret_node(n->n2);
		case P_LT:
			return interpret_node(n->n1) < interpret_node(n->n2);
		case P_SUB:
			return interpret_node(n->n1) - interpret_node(n->n2);
		case P_ADD:
			return interpret_node(n->n1) + interpret_node(n->n2);
		case P_CONST:
			return n->value;
		case P_VAR:
			return globals[n->value];
		case P_EXPR:
			return interpret_node(n->n1);
		case P_SEQ:
			if (n->n1)
				interpret_node(n->n1);
			return interpret_node(n->n2);
		case P_EMPTY:
			return 0;
		default:
			fprintf(stderr, "interpret_node(): ERROR: Unsupported node type %d\n", n->type);
			abort();
			return 0;
	}
}

/* DRIVER *********************************************************************/
int main(void)
{
	int i;
	generate(program());
	/*dump_program();*/
	run();

	for (i=0; i<26; i++)
		if (globals[i])
			printf("%c = %d\n", i+'a', globals[i]);
	return 0;
}
