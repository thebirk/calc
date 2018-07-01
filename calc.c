#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#ifdef _WIN32
#   define strdup _strdup
#endif /* _WIN32 */

#define new(T) ((T*) calloc(1, sizeof(T)))

typedef enum NodeKind {
	NODE_INVALID = 0,
    NODE_NUMBER,
    NODE_BINARY,
    NODE_UNARY,
	NODE_VARIABLE,
	NODE_UNKNOWN,
	NODE_FUNCTIONCALL,
	NODE_EOF,
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
	TOKEN_NUMBER = 128,
	TOKEN_EOF,
	TOKEN_UNKNOWN,
} TokenKind;

typedef struct Token {
	TokenKind kind;
	union {
		double number;
		char unknown_char;
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
} Calc;

typedef struct Args {
    char *filename; // If we specify a file, do we want to read that until we reach the end, then switch to stdin or just exit?
    bool  show_help;
} Args;

void print_help() {
    printf("calc [options] input\n");
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

Node* new_node(NodeKind kind) {
	Node *n = new(Node);
	n->kind = kind;
	return n;
}

Token get_token(Calc *calc) {
	while(*calc->input_text) {
		char c = *calc->input_text;

		if(c == '\n') {
			calc->input_text++;
			return (Token){.kind = TOKEN_EOF};
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
		}

		if(isdigit(c)) {
			char buffer[256];
			int offset = 0;
			while(isdigit(c)) {
				buffer[offset++] = c;
				calc->input_text++;
				c = *calc->input_text;
			}
			buffer[offset++] = 0;

			double num = strtod(buffer, 0);
			return (Token){.kind = TOKEN_NUMBER, .number = num};
		}

		printf("Unknown: '%c'/%d\n", c, c);
		return (Token){.kind = TOKEN_UNKNOWN, .unknown_char = c};
	}

	return (Token){.kind = TOKEN_EOF};
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

	if(match_token(calc, TOKEN_NUMBER)) {
		Node *n = new_node(NODE_NUMBER);
		n->number.number = t.number;
		return n;
	} else if(match_token(calc, TOKEN_UNKNOWN)) {
		Node *n = new_node(NODE_UNKNOWN);
		return n;
	} else if(match_token(calc, TOKEN_EOF)) {
		return new_node(NODE_EOF);
	} else if(match_token(calc, '(')) {
		Node *expr = parse_expr(calc);
		if(!match_token(calc, ')')){
			printf("Mismatched parens, rp.kind: '%c'/%d\n", calc->current_token.kind, calc->current_token.kind);
			return 0;
		}
		return expr;
	} else {
		return new_node(NODE_UNKNOWN);
	}	
}

Node* parse_base(Calc *calc) {
	Node *expr = parse_operand(calc);
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
		Node *unary = new_node(NODE_UNARY);
		unary->unary.op = unary_op;
		unary->unary.expr = rhs;
		rhs = unary;
	}

	return rhs;
}

Node* parse_mul(Calc *calc) {
	Node *lhs = parse_unary(calc);

	while(is_token(calc, '*') || is_token(calc, '/') || is_token(calc, '%')) {
		Token op = calc->current_token;
		next_token(calc);
		Node *rhs = parse_unary(calc);

		Node *bin = new_node(NODE_BINARY);
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

		Node *bin = new_node(NODE_BINARY);
		bin->binary.op = op.kind;
		bin->binary.lhs = lhs;
		bin->binary.rhs = rhs;
		lhs = bin;
	}

	return lhs;
}

Node* parse_expr(Calc *calc) {
	return parse_plus(calc);
}

Node* parse(Calc *calc) {
	// check for unary
	// parse term
	// while op do term
	// apply unary

	// when executing the node, if the node is a binary and the op is '=' assert that the lhs is a variable and assign the rhs to it
	return new(Node);
}

void indent(int *indent) {
	int max = *indent;
	for(int i = 0; i < max; i++) {
		printf("  ");
	}
}

void print_node(int *i, Node *n) {
	switch(n->kind) {
		case NODE_NUMBER: {
			indent(i);
			printf("NUMBER: %f\n", n->number.number);
		} break;
		case NODE_BINARY: {
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
		case NODE_UNARY: {
			indent(i);
			printf("UNARY: '%c'\n", n->unary.op);
			(*i)++;
			indent(i);
			printf("expr:\n");
			(*i)++;
			print_node(i, n->unary.expr);
			(*i)--;
		} break;
		default: {
			assert(!"Incomplete");
		} break;
	}
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
			//NOTE: I think it's better to use just "r" here and not "rb" as we dont really care about bytes, or line endings. We just want characters.
			calc.input = fopen(args.filename, "r");

		}
	} else {
		calc.input = stdin;
	}

	while(true) {
		printf("> ");

		char buffer[4096];
		char *input = fgets(buffer, 4096, calc.input);
		
		calc.input_text = input;

		if(strcmp(input, ".exit\n") == 0) {
			break;
		}

		next_token(&calc); // advance to first token
		Node *n = parse_expr(&calc);
		int indent = 0;
		print_node(&indent, n);
	}

	/*
	printf("> ");
	next_token(&calc);
	parse_operand(&calc);
	*/

	/*Token t = get_token(&calc);
	if(t.kind < 128) {
		printf("t.kind: '%c'/%d\n", t.kind, t.kind);
	} else if(t.kind == TOKEN_NUMBER) {
		printf("number: %f\n", t.number);
	} else if(t.kind == TOKEN_EOF) {
		printf("eof\n");
	} else if(t.kind ==TOKEN_UNKNOWN) {
		printf("unknown: '%c'\n", t.unknown_char);
	}*/

    return 0;
}