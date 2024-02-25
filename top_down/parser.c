#include <stdio.h>
#include <string.h>
#include <stdlib.h> // for exit
#include <ctype.h> // for isspace

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

typedef struct prod_rule {
  int num_symbols;
  char* sym_l[MAX_TERMS_PER_RULE];
} prod_rule;

typedef struct symbol {
  char* name;
  int num_prod_rules;
  int prod_rule_sel;
  struct prod_rule_l* prod_l;
} symbol;

typedef struct prod_rule_l {
  struct prod_rule* rule;
  struct prod_rule_l* next;
} prod_rule_l;

// top level list containing one item for every non-terminal definition
typedef struct symbol_l {
  struct symbol* sym;
  struct symbol_l* next;
} symbol_l;


typedef struct ptree_node {
  char *name;
  int idx_old;
  struct prod_rule_l* next_rule;
  struct ptree_node* parent;
  struct ptree_edge* edge_l;
} ptree_node;

typedef struct ptree_edge {
  struct ptree_node* node;
  struct ptree_edge* next;
} ptree_edge;

typedef struct node_l {
  struct ptree_node* node;
  struct node_l* next;
} node_l;



int is_terminal(const char* token, symbol_l* sym_list);
int get_token(FILE* in, char *buf);
void unget_token(FILE* in, char* token);
int get_token_from_string(const char* in, char* buf, int* isend);

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

prod_rule* get_prod_rule_by_idx(prod_rule_l* list, int idx);
symbol* get_symbol_by_name(symbol_l* list, const char* name);
ptree_node* parse_tree_gen(const char* in, symbol_l* sym_list);
void cleanup_tree(ptree_node* root, symbol_l* sym_list);
ptree_edge* insert_edge(ptree_edge* edge, ptree_node* node);
ptree_node* gen_ptree_node(symbol* sym, ptree_node* parent);
ptree_node* gen_ptree_node_for_terminal(const char* name, ptree_node* parent);
ptree_node* pop_node(node_l** stack);
node_l* remove_from_node_list(node_l* list, ptree_node* node);
node_l* push_node(node_l* list, ptree_node* node);
void print_tree(ptree_node* top);
void free_node_list(node_l* list);
void free_edges(ptree_edge* edge);
void free_tree(ptree_node* top);


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

  int num_read = fread(file_contents, sizeof(char), FSIZE, stdin);
  file_contents[num_read] = '\0';

  symbol_l* sym_list = generate_grammar_map(grammar_f);
  //print_grammar_map(sym_list);

  ptree_node* tree = parse_tree_gen(file_contents, sym_list);
  cleanup_tree(tree, sym_list);
  print_tree(tree);
  free_tree(tree);

  free_grammar_map(sym_list);

  fclose(grammar_f);
}



node_l* push_node(node_l* list, ptree_node* node)
{
  node_l* new = malloc(sizeof(node_l));
  new->node = node;
  new->next = list;
  return new;
}

ptree_node* pop_node(node_l** stack)
{
  if (*stack == NULL) {
    return NULL;
  }
  ptree_node* out = (*stack)->node;
  node_l* next = (*stack)->next;
  free(*stack);
  *stack = next;
  return out;
}

ptree_edge* insert_edge(ptree_edge* edge, ptree_node* node)
{
  ptree_edge* new = malloc(sizeof(ptree_edge));
  new->node = node;
  new->next = edge;
  return new;
}

ptree_node* gen_ptree_node(symbol* sym, ptree_node* parent)
{
  ptree_node* out = malloc(sizeof(ptree_node));
  out->next_rule = sym->prod_l;
  out->name = strdup(sym->name);
  out->parent = parent;
  if (parent != NULL)
    parent->edge_l = insert_edge(parent->edge_l, out);
  out->edge_l = NULL;
  return out;
}


ptree_node* gen_ptree_node_for_terminal(const char* name, ptree_node* parent)
{
  ptree_node* out = malloc(sizeof(ptree_node));
  out->next_rule = NULL;
  out->name = strdup(name);
  out->parent = parent;
  out->edge_l = NULL;
  // insert as child of parent node
  if (parent != NULL)
    parent->edge_l = insert_edge(parent->edge_l, out);
  return out;
}

ptree_node* parse_tree_gen(const char* in, symbol_l* sym_list)
{
  ptree_node* root;
  ptree_node* focus;
  ptree_node* tmp_node;
  ptree_edge* tmp_edge;
  node_l* stack;
  prod_rule* rule;
  symbol* sym_tmp;
  int i;
  char word[TOK_LEN], word_old[TOK_LEN];
  int isend;
  int idx, idx_old;


  stack = NULL;
  root = gen_ptree_node(sym_list->sym, NULL); // generate root node with no parent
  root->idx_old = 0;
  focus = root;
  idx_old = 0;
  idx = get_token_from_string(in, word, &isend);
  strcpy(word_old, word);
  while (1) {
    if (isend && focus == NULL) {
      return root;
    } if (focus != NULL && !(is_terminal(focus->name, sym_list))) {
      rule = focus->next_rule->rule;
      focus->next_rule = focus->next_rule->next;
      if (rule->num_symbols == 0) {
        focus = pop_node(&stack);
        if (focus != NULL)
          focus->idx_old = idx_old;
        continue;
      }

      // put all symbols except the first one onto the stack
      for (i = rule->num_symbols-1; i >= 1; i--) {
        sym_tmp = get_symbol_by_name(sym_list, rule->sym_l[i]);
        if (sym_tmp == NULL) { // terminals will not be in the symbol list
                               // insert them as literals
          tmp_node = gen_ptree_node_for_terminal(rule->sym_l[i], focus);
        } else {
          tmp_node = gen_ptree_node(sym_tmp, focus);
        }
        stack = push_node(stack, tmp_node);
      }

      // has to have at least one symbol
      // choose first symbol in production rule as new focus
      sym_tmp = get_symbol_by_name(sym_list, rule->sym_l[0]);
      if (sym_tmp == NULL) {
        focus = gen_ptree_node_for_terminal(rule->sym_l[0], focus);
      } else {
        focus = gen_ptree_node(sym_tmp, focus);
      }
      focus->idx_old = idx_old;

    } else if (focus != NULL && strcmp(focus->name, word) == 0) {
      strcpy(word_old, word);
      idx_old = idx;
      idx += get_token_from_string(in + idx, word, &isend);

      focus = pop_node(&stack);
      if (focus != NULL)
        focus->idx_old = idx_old;

    } else {
      do {
        if (focus != NULL) {
          focus = focus->parent;
        }
        if (focus == NULL) {
          fprintf(stderr, "Parsing error. Exiting...\n");
          exit(1);
        }

        for (tmp_edge = focus->edge_l->next; tmp_edge != NULL; tmp_edge = tmp_edge->next) {
          stack = remove_from_node_list(stack, tmp_edge->node);
        }
        free_edges(focus->edge_l);
        focus->edge_l = NULL;

        if (focus == NULL) {
          fprintf(stderr, "Matching error: could not match input with grammar\n");
          exit(1);
        }
      } while (focus->next_rule == NULL);
      idx = focus->idx_old;
      idx_old = idx;
      idx += get_token_from_string(in + idx, word, &isend);
    }
  }
  free_node_list(stack);
  return root;
}

void cleanup_tree(ptree_node* root, symbol_l* sym_list)
{
  ptree_edge* tmp_edge;
  ptree_edge* prev_edge;
  ptree_edge* next_edge;

  prev_edge = root->edge_l;
  // it can't be the first node since there has to be at least one child
  // per non-terminal node and for terminal nodes there are no children
  // to remove
  if (root->edge_l != NULL) {
    cleanup_tree(root->edge_l->node, sym_list);
    for (tmp_edge = root->edge_l->next; tmp_edge != NULL; tmp_edge = next_edge) {
      next_edge = tmp_edge->next;
      if (tmp_edge->node->edge_l == NULL
          && !(is_terminal(tmp_edge->node->name, sym_list))) {
        // remove this item from the children's list
        free(tmp_edge->node->name);
        free(tmp_edge->node);
        free(tmp_edge);
        prev_edge->next = next_edge;
      } else {
        cleanup_tree(tmp_edge->node, sym_list);
        prev_edge = tmp_edge;
      }
    }
  }
}

void print_tree(ptree_node* top)
{
  ptree_edge* tmp_edge;
  
  if (top != NULL) {
    printf("Node: %s\n", top->name);
    for (tmp_edge = top->edge_l; tmp_edge != NULL; tmp_edge = tmp_edge->next) {
      printf("Child node: %s\n", tmp_edge->node->name);
    }
    // do recursion in separate loop to first print all child nodes
    // which is easier to understand
    for (tmp_edge = top->edge_l; tmp_edge != NULL; tmp_edge = tmp_edge->next) {
      print_tree(tmp_edge->node);
    }
  }
}

// this will not free the node pointed to by the list entry
node_l* remove_from_node_list(node_l* list, ptree_node* node)
{
  node_l* out;
  if (list == NULL) {
    return NULL;
  } else if (list->node == node) {
    out = list->next;
    free(list);
    return out;
  } else {
    out = list;
    for (list = out; list->next != NULL; list = list->next) {
      if (list->next->node == node) {
        free(list->next);
        list->next = list->next->next;
        return out;
      }
    }
    return out; // if no match was found
  }
}


void free_node_list(node_l* list)
{
  node_l* next;
  for (; list != NULL; list = next) {
    next = list->next;
    free_tree(list->node);
    free(list);
  }
}

void free_edges(ptree_edge* edge)
{
  ptree_edge* next_edge;

  for (; edge != NULL; edge = next_edge) {
    next_edge = edge->next;
    free_tree(edge->node);
    free(edge);
  }
}


void free_tree(ptree_node* top)
{
  if (top != NULL) {
    free_edges(top->edge_l);
    free(top->name);
    free(top);
  }
}

// check if symbol with name 'name' is in the symbol list 'list'
symbol* get_symbol_by_name(symbol_l* list, const char* name)
{
  symbol_l* iter;
  for (iter = list; iter != NULL; iter = iter->next) {
    if (strcmp(iter->sym->name, name) == 0)
      return iter->sym;
  }
  return NULL;
}

prod_rule* get_prod_rule_by_idx(prod_rule_l* list, int idx)
{
  prod_rule_l* iter;
  for (iter = list; iter != NULL; iter = iter->next) {
    if (idx-- == 0)
      return iter->rule;
  }
  return NULL;
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

// check whether it is in given sym list
int is_terminal(const char* token, symbol_l* sym_list)
{
  symbol_l* iter;
  for (iter = sym_list; iter != NULL; iter = iter->next) {
    if (strcmp(iter->sym->name, token) == 0)
      return 0;
  }
  return 1;
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
  } while (in_c != EOF);

  buf[buf_idx] = '\0';
  return (in_c == EOF) ? EOF : buf_idx;
}

// return the index of where inside in you are now or -1 for end of string
int get_token_from_string(const char* in, char* buf, int* isend)
{
  int idx, buf_idx;

  idx = -1;
  buf_idx = 0;
  while (isspace(in[++idx]));
  if (in[idx] == '\0') {
    *isend = 1;
    return idx;
  } else {
    buf[buf_idx++] = in[idx];
  }

  do {
    if (!isspace(in[++idx]))
      buf[buf_idx++] = in[idx];
    else
      break;
  } while (in[idx] != '\0');

  buf[buf_idx] = '\0';
  *isend = (in[idx] == '\0') ? 1 : 0;
  return idx;
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
