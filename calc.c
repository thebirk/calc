#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

/*

v0.02 - Added support for decimal point

TODO:
	- Lex numbers properly, allow for decimals, hexadecimals, binary and scientific
	- Add function call
	- Add all kinds of common math function

PERHAPS TODOS:
	- User functions f(x) := x * x
	- Redo last expr, probably not needed when we get history on non-windows systems
*/

#define CALC_VERISON "v0.02"
#include "CALC_BUILD.h"

#ifdef _WIN32
#   define strdup _strdup
#endif /* _WIN32 */

#define new(T) ((T*) calloc(1, sizeof(T)))

typedef enum NodeKind {
	NodeInvalid = 0,
	NodeNumber,
	NodeBinary,
	NodeUnary,
	NodeVariable,
	NodeUnknown,
	NodeFunctioncall,
	NodeEof,
} NodeKind;

typedef struct Node Node;
struct Node {
	NodeKind kind;
	union {
		struct {
			double number;
		} number;
		struct {
			char op;
			Node *lhs;
			Node *rhs;
		} binary;
		struct {
			char op;
			Node *expr;
		} unary;
		struct {
			char *name;
		} variable;
		struct {
			char *func_name;
			Node **args;
			int num_args;
		} function_call;
	};
};

typedef enum TokenKind {
	TokenNumber = 128,
	TokenEof,
	TokenUnknown,
	TokenName,
} TokenKind;

typedef struct Token {
	TokenKind kind;
	union {
		double number;
		char unknown_char;
		char *name;
	};
} Token;

typedef struct Variable {
	char  *name;
	double value;
} Variable;

typedef struct Calc {
	FILE     *input;
	Variable *variables;
	int       num_variables;
	Token     current_token;
	char     *input_text;
	char     *input_text_ptr;
} Calc;

typedef struct Args {
	char *filename; // If we specify a file, do we want to read that until we reach the end, then switch to stdin or just exit?
	bool  show_help;
} Args;

void print_help() {
	printf("calc [options or input]\n");
	printf("Default input is '-', stdin\n");
	printf("\nOptions:\n");
	printf("\t-h/help - Prints out this\n");
}

void parse_args(int argc, const char **argv, Args *args) {
	if(argc < 2) {
		return;
	}

	for(int i = 1; i < argc; i++) {
		char *arg = (char*) argv[i];
		if(*arg == '-') {
			char *name = arg+1;
			if(name && *name) {
				if(strcmp(name, "h") == 0 || strcmp(name, "help") == 0) {
					args->show_help = true;
				} else {
					fprintf(stderr, "Unknown option '%s'\n", arg);
					print_help();
					exit(1);
				}
			} else {
				if(args->filename) {
					fprintf(stderr, "Multiple filenames passed. Already got '%s', found '%s'", args->filename, arg);
					exit(1);
				} else {
					args->filename = strdup(arg);
				}
			}
		} else {
			if(args->filename) {
				fprintf(stderr, "Multiple filenames passed. Already got '%s', found '%s'", args->filename, arg);
				exit(1);
			} else {
				args->filename = strdup(arg);
			}
		}
	}
}

#define new_node(T) (_new_node(Node ## T))
Node* _new_node(NodeKind kind) {
	Node *n = new(Node);
	n->kind = kind;
	return n;
}

Token get_token(Calc *calc) {
	char c = *calc->input_text;

	if(c == 0) {
		return (Token){.kind = TokenEof};
	}

	if(c == '\n') {
		calc->input_text++;
		return (Token){.kind = TokenEof};
	}

	if(c == '\r' || c == ' ' || c == '\t') {
		calc->input_text++;
		return get_token(calc);
	}

	switch(c) {
		case '+': calc->input_text++; return (Token){.kind = '+'};
		case '-': calc->input_text++; return (Token){.kind = '-'};
		case '/': calc->input_text++; return (Token){.kind = '/'};
		case '*': calc->input_text++; return (Token){.kind = '*'};
		case '%': calc->input_text++; return (Token){.kind = '%'};
		case '^': calc->input_text++; return (Token){.kind = '^'};
		case '(': calc->input_text++; return (Token){.kind = '('};
		case ')': calc->input_text++; return (Token){.kind = ')'};
		case '=': calc->input_text++; return (Token){.kind = '='};
	}

	if(isdigit(c)) {
		char buffer[256];
		int offset = 0;
		bool found_dot = false;

		while(isdigit(c) || c == '.') {
			if(c == '.') {
				if(found_dot) break;
				found_dot = true;
			}
			buffer[offset++] = c;
			calc->input_text++;
			c = *calc->input_text;
		}
		buffer[offset++] = 0;

		double num = strtod(buffer, 0);
		return (Token){.kind = TokenNumber, .number = num};
	}
	
	if(isalpha(c) || c == '$' || c == '_' || c == '#') {
		char buffer[256];
		int offset = 0;
		while(isalnum(c) || c == '$' || c == '_' || c == '#') {
			buffer[offset++] = c;
			calc->input_text++;
			c = *calc->input_text;
		}
		buffer[offset++] = 0;

		char *name = strdup(buffer); //TODO: Fix leak
		return (Token){.kind = TokenName, .name = name};
	}

	printf("Unknown: '%c'/%d\n", c, c);
	return (Token){.kind = TokenUnknown, .unknown_char = c};
}

Token next_token(Calc *calc) {
	calc->current_token = get_token(calc);
	return calc->current_token;
}

bool is_token(Calc *calc, TokenKind kind) {
	if(calc->current_token.kind == kind) {
		return true;
	}
	return false;
}

bool match_token(Calc *calc, TokenKind kind) {
	if(calc->current_token.kind == kind) {
		next_token(calc);
		return true;
	}
	return false;
}

Node* parse_expr(Calc *calc);
Node* parse_operand(Calc *calc) {
	Token t = calc->current_token;

	if(match_token(calc, TokenNumber)) {
		Node *n = new_node(Number);
		n->number.number = t.number;
		return n;
	} else if(match_token(calc, TokenName)) {
		Node *n = new_node(Variable);
		n->variable.name = t.name; //TODO: To we need  to copy the name here, or is it safe to assume that we own it?
		return n;
	} else if(match_token(calc, TokenUnknown)) {
		Node *n = new_node(Unknown);
		printf("Unknown token: '%c'/%d\n", t.kind, t.kind);
		return n;
	} else if(match_token(calc, TokenEof)) {
		printf("Unexptected end of input\n");
		return new_node(Eof);
	} else if(match_token(calc, '(')) {
		Node *expr = parse_expr(calc);
		if(!match_token(calc, ')')){
			printf("Mismatched parens, rp.kind: '%c'/%d\n", calc->current_token.kind, calc->current_token.kind);
			return 0;
		}
		return expr;
	} else {
		printf("Unexpected token: '%c'/%d\n", t.kind, t.kind);
		return new_node(Unknown);
	}	
}

Node* parse_base(Calc *calc) {
	Node *expr = parse_operand(calc);

	while(is_token(calc, '(')) {
		Token op = calc->current_token;

		if(match_token(calc, '(')) {
			assert(!"Incomplete!");
		} else {
			assert(!"Invalid case");
		}
	}

	return expr;
}

Node* parse_unary(Calc *calc) {
	bool is_unary = false;
	char unary_op = 0;
	Token op = calc->current_token;
	if(op.kind == '+' || op.kind == '-') {
		is_unary = true;
		unary_op = op.kind;
		next_token(calc);
	}

	Node *rhs = parse_base(calc);
	
	if(is_unary) {
		Node *unary = new_node(Unary);
		unary->unary.op = unary_op;
		unary->unary.expr = rhs;
		rhs = unary;
	}

	return rhs;
}

Node* parse_pow(Calc *calc) {
	Node *lhs = parse_unary(calc);

	while(is_token(calc, '^')) {
		Token op = calc->current_token;
		next_token(calc);
		Node *rhs = parse_unary(calc);

		Node *bin = new_node(Binary);
		bin->binary.op = op.kind;
		bin->binary.lhs = lhs;
		bin->binary.rhs = rhs;
		lhs = bin;
	}

	return lhs;
}

Node* parse_mul(Calc *calc) {
	Node *lhs = parse_pow(calc);

	while(is_token(calc, '*') || is_token(calc, '/') || is_token(calc, '%')) {
		Token op = calc->current_token;
		next_token(calc);
		Node *rhs = parse_pow(calc);

		Node *bin = new_node(Binary);
		bin->binary.op = op.kind;
		bin->binary.lhs = lhs;
		bin->binary.rhs = rhs;
		lhs = bin;
	}

	return lhs;
}

Node* parse_plus(Calc *calc) {
	Node *lhs = parse_mul(calc);

	while(is_token(calc, '+') || is_token(calc, '-')) {
		Token op = calc->current_token;
		next_token(calc);
		Node *rhs = parse_mul(calc);

		Node *bin = new_node(Binary);
		bin->binary.op = op.kind;
		bin->binary.lhs = lhs;
		bin->binary.rhs = rhs;
		lhs = bin;
	}

	return lhs;
}

Node* parse_assign(Calc *calc) {
	Node *lhs = parse_plus(calc);

	if(is_token(calc, '=')) {
		Token op = calc->current_token;
		next_token(calc);
		Node *rhs = parse_plus(calc);

		Node *bin = new_node(Binary);
		bin->binary.op = op.kind;
		bin->binary.lhs = lhs;
		bin->binary.rhs = rhs;
		lhs = bin;
	}

	return lhs;
}

Node* parse_expr(Calc *calc) {
	return parse_assign(calc);
}

void indent(int *indent) {
	int max = *indent;
	for(int i = 0; i < max; i++) {
		printf("  ");
	}
}

void print_node(int *i, Node *n) {
	switch(n->kind) {
		case NodeNumber: {
			indent(i);
			printf("NUMBER: %f\n", n->number.number);
		} break;
		case NodeBinary: {
			indent(i);
			printf("BINARY: '%c'\n", n->binary.op);
			(*i)++;
			indent(i);
			printf("lhs:\n");
			(*i)++;
			print_node(i, n->binary.lhs);
			(*i)--;

			indent(i);
			printf("rhs:\n");
			(*i)++;
			print_node(i, n->binary.rhs);
			(*i)--;
			(*i)--;
		} break;
		case NodeUnary: {
			indent(i);
			printf("UNARY: '%c'\n", n->unary.op);
			(*i)++;
			indent(i);
			printf("expr:\n");
			(*i)++;
			print_node(i, n->unary.expr);
			(*i)--;
		} break;
	}
}

double binary_op(int op, double lhs, double rhs) {
	switch(op) {
		case '+': return lhs + rhs;
		case '-': return lhs - rhs;
		case '*': return lhs * rhs;
		case '/': return lhs / rhs;
		case '%': return fmod(lhs, rhs);
		case '^': return pow(lhs, rhs);

		default: {
			assert(!"Invalid case");
			return 12345678.0;
		}
	}
}

Variable* get_variable(Calc *calc, char *name) {
	if(calc->num_variables == 0) return 0;
	for(int i = 0; i < calc->num_variables; i++) {
		if(strcmp(name, calc->variables[i].name) == 0) {
			return &calc->variables[i];
		}
	}
	return 0;
}

bool add_variable(Calc *calc, char *name, double value) {
	if(get_variable(calc, name) != 0) {
		return false;
	}

	calc->num_variables++;
	calc->variables = realloc(calc->variables, sizeof(Variable)*calc->num_variables);
	calc->variables[calc->num_variables-1] = (Variable){.name = strdup(name), .value = value};
	return true;
}

bool eval_expr(Calc *calc, Node *n, double *result) {
	assert(result);

	switch(n->kind) {
		case NodeNumber: {
			*result = n->number.number;
			return true;
		} break;
		case NodeBinary: {
			double lhs, rhs;

			if(n->binary.op == '=') {
				if(n->binary.lhs->kind != NodeVariable) {
					printf("cannot assign to left hand side of expression\n");
					return false;
				}

				double res = 0;
				bool ok = eval_expr(calc, n->binary.rhs, &res);
				if(!ok) return false;

				if(n->binary.lhs->variable.name && *n->binary.lhs->variable.name == '#') {
					printf("reserved variable name\n");
					return false;
				}

				Variable *v = get_variable(calc, n->binary.lhs->variable.name);
				if(!v) {
					ok = add_variable(calc, n->binary.lhs->variable.name, res);
					if(!ok) {
						printf("failed to add variable '%s'\n", n->binary.lhs->variable.name);
						return false;
					}

					*result = res;
					return true;
				}
				v->value = res;

				*result = res;
				return true;
			} else {
				bool ok = false;
				ok = eval_expr(calc, n->binary.lhs, &lhs);
				if(!ok) return false;
				ok = eval_expr(calc, n->binary.rhs, &rhs);
				if(!ok) return false;

				*result = binary_op(n->binary.op, lhs, rhs);

				return true;
			}
		} break;
		case NodeUnary: {
			double rhs;
			bool ok = eval_expr(calc, n->unary.expr, &rhs);
			if(!ok) return false;

			switch(n->unary.op) {
				case '+': *result = +rhs; break; // Do we really need this?
				case '-': *result = -rhs; break;

				default: {
					assert(!"Invalid unary op");
					return false;
				}
			}

			return true;
		} break;
		case NodeEof: {
			printf("expression error\n");
			return false;
		} break;
		case NodeInvalid: {
			printf("expression error\n");
			return false;
		} break;
		case NodeUnknown: {
			printf("expression error\n");
			return false;
		} break;
		case NodeVariable: {
			Variable *v = get_variable(calc, n->variable.name);
			if(v) {
				*result = v->value;
				return true;
			} else {
				printf("unknown variable '%s'\n", n->variable.name);
				return false;
			}
		} break;

		default: {
			printf("n->kind: %d\n", n->kind);
			assert(!"Missing case!");
			return false;
		} break;
	}

	assert(!"Nope");
}

void free_node(Node *n) {
	switch(n->kind) {
		case NodeBinary: {
			free_node(n->binary.lhs);
			free_node(n->binary.rhs);
		} break;
		case NodeVariable: {
			free(n->variable.name);
		} break;
		case NodeUnary: {
			free_node(n->unary.expr);
		} break;
		case NodeFunctioncall: {
			assert(!"Incomplete!");
		} break;
	}

	free(n);
}

#if 1
char* get_input(Calc *calc) {
	char buffer[4096];
	char *input = fgets(buffer, 4096, calc->input);
	if(!input) {
		calc->input = stdin;
		return get_input(calc);
	}

	//TODO: Strip newline from end, if there is one

	char *result = strdup(input);
	return result;	
}

void free_input(Calc *calc) {
	free(calc->input_text_ptr);
}

#else

//TODO: Find out what lib to use for non windows input
#include "linenoise/linenoise.h"
#include "linenoise/linenoise.c"
char* get_input(Calc *calc) {
	return linenoise("");
}

void free_input(Calc *calc) {
	linenoiseFree(calc->input_text);
}

#endif /* _WIN32 vs. linenoise*/

void print_calc_help() {
	printf("Operators:\n");
	printf("  + - addition\n");
	printf("  - - subtraction\n");
	printf("  * - multiplication\n");
	printf("  / - division\n");
	printf("  %% - modulo\n");
	printf("  ^ - exponentiation\n");

	printf("\nCommands\n");
	printf("  help      - show this message\n");
	printf("  cls/clear - clear screen\n");
	printf("  clc       - clear memory\n");
	printf("  exit/q    - quit calc\n");
}

int main(int argc, const char **argv) {
	Args args = {0};
	parse_args(argc, argv, &args);

	if(args.show_help) {
		print_help();
		return 0;
	}

	Calc calc = {0};
	if(args.filename) {
		if(strcmp(args.filename, "-") == 0) {
			calc.input = stdin;
		} else {
			calc.input = fopen(args.filename, "r");

		}
	} else {
		calc.input = stdin;
	}

	printf("calc "CALC_VERISON" build "CALC_BUILD"\n");
	printf("Type 'exit' or 'q' to quit, and 'help' for help\n\n");

	int R = 0;

	while(true) {
		printf("> ");
		fflush(stdout);

		if(calc.input_text) free_input(&calc);
		char *input = get_input(&calc);
		calc.input_text_ptr = calc.input_text = input;

		if(strcmp(input, "exit\n") == 0 || strcmp(input, "q\n") == 0) {
			break;
		} else if(strcmp(input, "cls\n") == 0 || strcmp(input, "clear\n") == 0) {
			#ifdef _WIN32
				system("cls");
			#else
				system("clear");
			#endif

			continue;
		} else if(strcmp(input, "help\n") == 0) {
			print_calc_help();
			continue;
		} else if(strcmp(input, "clc\n") == 0) {
			for(int i = 0; i < calc.num_variables; i++) {
				free(calc.variables[i].name);
			}
			free(calc.variables);
			calc.num_variables = 0;
			calc.variables = 0;
			R=0;
			continue;
		}

		next_token(&calc);
		Node *n = parse_expr(&calc);
		if(calc.current_token.kind != TokenEof) {
			printf("expression error\n");
			continue;
		}
		
		double result = 0;
		bool ok = eval_expr(&calc, n, &result);
		if(ok) {
			R++;

			Variable *rvar = get_variable(&calc, "#");
			if(!rvar) {
				if(!add_variable(&calc, "#", result)) {
					printf("Failed to add variable '%s'\n", "#");
				}
			} else {
				rvar->value = result;
			}

			printf("#%d: %f\n", R, result);
			char buffer[256];
			snprintf(buffer, 256, "#%d", R);
			char *name = strdup(buffer);
			if(!add_variable(&calc, name, result)) {
				printf("Failed to add variable '%s'\n", name);
			}
		}

		//int indent = 0;		
		//print_node(&indent, n);

		free_node(n);
	}

	return 0;
}
