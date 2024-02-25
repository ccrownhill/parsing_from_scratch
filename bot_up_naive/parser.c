#include <stdio.h>
#include <string.h>
#include <stdlib.h> // for exit
#include <ctype.h> // for isspace

#include "parser.h"

#define TOK_LEN 50
#define MAX_TERMS_PER_RULE 10
#define FSIZE 1000

#define APPEND(LIST, LAST, NEW) ({\
  if (list != NULL) {\
    for (last = list; last->next != NULL; last = last->next);\
      last->next = new;\
    return list;\
  } else {\
    return new;\
  }\
})

typedef struct _prod_rule {
  int num_symbols;
  char* sym_l[MAX_TERMS_PER_RULE];
} prod_rule;

typedef struct _symbol {
  char* name;
  int num_prod_rules;
  int prod_rule_sel;
  struct _prod_rule_l* prod_l;
} symbol;

typedef struct _prod_rule_l {
  struct _prod_rule* rule;
  struct _prod_rule_l* next;
} prod_rule_l;

// top level list containing one item for every non-terminal definition
typedef struct _symbol_l {
  struct _symbol* sym;
  struct _symbol_l* next;
} symbol_l;


typedef struct _str_stack {
  char* str;
  struct _str_stack* next;
} str_stack;


int get_token(FILE* in, char *buf);
void unget_token(FILE* in, char* token);

symbol* generate_symbol(char* name);
prod_rule* generate_prod_rule();
prod_rule_l* append_prod_rule_l(prod_rule_l* list, prod_rule* rule);
symbol_l* append_symbol_l(symbol_l* list, symbol* sym);
void generate_prod_rule_list(FILE* g_file, symbol* sym);
symbol_l* generate_grammar_map(FILE* g_file);
void print_prod_rule_list(prod_rule_l* list);
void print_grammar_map(symbol_l* gmap);

void free_prod_l(prod_rule_l* prod_l);
void free_grammar_map(symbol_l* gmap);


//ptree_node* parse_tree_gen(const char* in, symbol_l* sym_list);
int grammar_check(symbol_l* sym_list);

void free_str_stack(str_stack* stack);

str_stack* push_str_stack(str_stack* stack, const char* str);
char* pop_str_stack(str_stack** stack);

int check_for_handle(str_stack* stack, symbol_l* sym_list, char* nt_name);
int match_rule(str_stack* stack, prod_rule* rule);

char *terminals[] = {"NONE", "T_PLUS", "T_MINUS", "T_TIMES", "T_LBRACKET",
    "T_RBRACKET", "T_NUMBER"};


int main(int argc, char* argv[])
{
  char file_contents[FSIZE];

  if (argc < 2) {
    fprintf(stderr, "Usage: parser grammar_file [parse_file]\n");
    exit(1);
  }

  FILE* grammar_f;
  if (!(grammar_f = fopen(argv[1], "r"))) {
    fprintf(stderr, "Error: can't open file '%s'\n", argv[1]);
    exit(1);
  }

  symbol_l* sym_list = generate_grammar_map(grammar_f);
  print_grammar_map(sym_list);

  if (grammar_check(sym_list)) {
    printf("Grammar correct\n");
  } else {
    printf("Grammar incorrect\n");
  }

  free_grammar_map(sym_list);

  fclose(grammar_f);
}


char* pop_str_stack(str_stack** stack)
{
  if (*stack == NULL) {
    return NULL;
  }
  char* out = (*stack)->str;
  str_stack* next = (*stack)->next;
  fprintf(stderr, "popping: '%s'\n", out);
  free((*stack)->str);
  free(*stack);
  *stack = next;
  return out;
}

str_stack* push_str_stack(str_stack* stack, const char* str)
{
  str_stack* new = malloc(sizeof(str_stack));
  new->str = strdup(str);
  new->next = stack;
  return new;
}

int grammar_check(symbol_l* sym_list)
{
  str_stack* stack;
  tok_type_t tok_t;
  int handle_len;
  char nt_name[TOK_LEN];
  char debug[TOK_LEN];

  stack = push_str_stack(NULL, "invalid");
  tok_t = yylex();
  while (strcmp(stack->str, sym_list->sym->name) != 0 || tok_t != NONE) {
    if (handle_len = check_for_handle(stack, sym_list, nt_name)) {
      while (handle_len--) {
        pop_str_stack(&stack);
      }
      stack = push_str_stack(stack, nt_name);
    } else if (tok_t != NONE) {
      stack = push_str_stack(stack, terminals[tok_t]);
      tok_t = yylex();
    } else {
      free_str_stack(stack);
      return 0;
    }
  }

  // check whether stack is empty
  if (stack->next != NULL && strcmp(stack->next->str, "invalid") == 0) {
    free_str_stack(stack);
    return 1;
  } else {
    free_str_stack(stack);
    return 0;
  }
}

int check_for_handle(str_stack* stack, symbol_l* sym_list, char* nt_name)
{
  symbol_l* sym_iter;
  prod_rule_l* rule_iter;
  for (sym_iter = sym_list; sym_iter != NULL; sym_iter = sym_iter->next) {
    for (rule_iter = sym_iter->sym->prod_l; rule_iter != NULL; rule_iter = rule_iter->next) {
      if (match_rule(stack, rule_iter->rule)) {
        strncpy(nt_name, sym_iter->sym->name, TOK_LEN);
        return rule_iter->rule->num_symbols;
      }
    }
  }
  return 0;
}

int match_rule(str_stack* stack, prod_rule* rule)
{
  str_stack* stack_iter;
  int i;

  stack_iter = stack;
  for (i = rule->num_symbols-1; i >= 0 && strcmp(stack_iter->str, "invalid") != 0;
      i--, stack_iter = stack_iter->next) {
    if (strcmp(rule->sym_l[i], stack_iter->str) != 0) {
      break;
    }
  }

  return (i == -1);
}

void free_str_stack(str_stack* stack)
{
  str_stack* next;
  for (; stack != NULL; stack = next) {
    next = stack->next;
    free(stack->str);
    free(stack);
  }
}

void print_grammar_map(symbol_l* gmap)
{
  symbol_l* sym_iter;
  for (sym_iter = gmap; sym_iter != NULL; sym_iter = sym_iter->next) {
    printf("Non-terminal: '%s'\n", sym_iter->sym->name);
    print_prod_rule_list(sym_iter->sym->prod_l);
  }
}

void print_prod_rule_list(prod_rule_l* list)
{
  prod_rule_l* rule_iter;
  int i;

  for (rule_iter = list; rule_iter != NULL; rule_iter = rule_iter->next) {
    printf("Rule: ");
    for (i = 0; i < rule_iter->rule->num_symbols; i++) {
      printf(" '%s' ", rule_iter->rule->sym_l[i]);
    }
    printf("\n");
  }
}


void free_prod_l(prod_rule_l* prod_l)
{
  prod_rule_l* next;
  int i;

  for (; prod_l != NULL; prod_l = next) {
    next = prod_l->next;
    for (i = 0; i < prod_l->rule->num_symbols; i++) {
      free(prod_l->rule->sym_l[i]);
    }
    free(prod_l->rule);
    free(prod_l);
  }
}

void free_grammar_map(symbol_l* gmap)
{
  symbol_l* next;
  for (; gmap != NULL; gmap = next) {
    next = gmap->next;
    free_prod_l(gmap->sym->prod_l);
    free(gmap->sym->name);
    free(gmap->sym);
    free(gmap);
  }
}


// initialize symbol with no production rules
symbol* generate_symbol(char* name)
{
  symbol* out = malloc(sizeof(symbol));
  out->name = strdup(name);
  out->num_prod_rules = 0;
  out->prod_rule_sel = -1;
  out->prod_l = NULL;
  return out;
}

// initialize empty production rule
prod_rule* generate_prod_rule()
{
  prod_rule* out = malloc(sizeof(prod_rule));
  out->num_symbols = 0;
  return out;
}

prod_rule_l* append_prod_rule_l(prod_rule_l* list, prod_rule* rule)
{
  prod_rule_l* last;
  prod_rule_l* new = malloc(sizeof(prod_rule_l));
  new->rule = rule;
  new->next = NULL;

  APPEND(list, last, new);
}

symbol_l* append_symbol_l(symbol_l* list, symbol* sym)
{
  symbol_l* last;
  symbol_l* new = malloc(sizeof(symbol_l));
  new->sym = sym;
  new->next = NULL;
  
  APPEND(list, last, new);
}


void generate_prod_rule_list(FILE* g_file, symbol* sym)
{
  char token[TOK_LEN];
  prod_rule* tmp_rule;
  
  sym->num_prod_rules = 0;
  sym->prod_l = NULL;

  tmp_rule = generate_prod_rule();

  while (get_token(g_file, token) != EOF && token[strlen(token)-1] != ':') {
    if (token[0] != '|') {
      if (tmp_rule->num_symbols < MAX_TERMS_PER_RULE) {
        tmp_rule->sym_l[tmp_rule->num_symbols++] = strdup(token);
      } else {
        fprintf(stderr, "Grammar parse error: exceeded maximum number of symbols"
            "per production rule.\n"
            "Can't be over '%d'\n", MAX_TERMS_PER_RULE);
        exit(1);
      }
    } else {
      sym->prod_l = append_prod_rule_l(sym->prod_l, tmp_rule);
      sym->num_prod_rules++;
      tmp_rule = generate_prod_rule();
    }
  }
  sym->prod_l = append_prod_rule_l(sym->prod_l, tmp_rule);
  sym->num_prod_rules++;
  if (token[strlen(token)-1] == ':')
    unget_token(g_file, token);
}



symbol_l* generate_grammar_map(FILE* g_file)
{
  char token[TOK_LEN];
  symbol* tmp_sym;
  symbol_l* sym_list = NULL;
  while (get_token(g_file, token) != EOF) {
    if (token[strlen(token)-1] == ':') {
      token[strlen(token)-1] = '\0';
      tmp_sym = generate_symbol(token);
      generate_prod_rule_list(g_file, tmp_sym);
      sym_list = append_symbol_l(sym_list, tmp_sym);
    } else {
      fprintf(stderr, "syntax error: non-terminals must be defined with a"
          "':' without spaces separating it from the non-terminal name\n");
      exit(1);
    }
  }
  return sym_list;
}

// assume that all tokens are separated by spaces
int get_token(FILE* in, char *buf)
{
  char in_c;
  int buf_idx = 0;

  while (isspace(in_c = fgetc(in))); // skip all whitespace characters

  if (in_c == EOF)
    return EOF;
  else
    buf[buf_idx++] = in_c;

  do {
    if (!isspace(in_c = fgetc(in)))
      buf[buf_idx++] = in_c;
    else
      break;
    if (in_c == EOF)
      fprintf(stderr, "EOF\n");
  } while (in_c != EOF);

  buf[buf_idx] = '\0';
  return (in_c == EOF) ? EOF : buf_idx;
}

void unget_token(FILE* in, char* token)
{
  if (ungetc(' ', in) == EOF) {
    fprintf(stderr, "couldn't unget character ' '\n");
    exit(1);
  }
  for (int i = strlen(token) - 1; i >= 0; i--) {
    if (ungetc(token[i], in) == EOF) {
      fprintf(stderr, "couldn't unget character '%c'\n", token[i]);
      exit(1);
    }
  }
}
