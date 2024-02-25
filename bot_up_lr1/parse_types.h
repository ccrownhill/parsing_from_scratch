#ifndef PARSE_TYPES_H
#define PARSE_TYPES_H

#include "parser.h"
#include "util_types.h"

#define APPEND(LIST, LAST, NEW) ({\
  if (LIST != NULL) {\
    for (LAST = LIST; LAST->next != NULL; LAST = LAST->next);\
    LAST->next = NEW;\
    return LIST;\
  } else {\
    return NEW;\
  }\
})

#define TOK_LEN 50
#define MAX_TERMS_PER_RULE 10

typedef struct _ProdRule {
  char *sym;
  int num_symbols;
  char *sym_l[MAX_TERMS_PER_RULE];
  struct _ProdRule *next;
} ProdRule;


typedef struct _FirstSetEl {
  TokType token_type;
  struct _FirstSetEl *next;
} FirstSetEl;


// LR1 element to make up linked list representing a set in the
// canonical collection of sets
typedef struct _LR1El {
  ProdRule *rule;
  int pos;
  TokType lookahead;
  struct _LR1El *next;
} LR1El;


typedef struct _CC {
  int state_no;
  LR1El *cc_set;
  HashMap *goto_map;
  struct _CC *next;
} CC;

typedef struct _CCStack {
  CC *cc;
  struct _CCStack *next;
} CCStack;



typedef enum _ActionType {SHIFT, REDUCE, ACCEPT} ActionType;

typedef union _ActionInstr {
  int state_no;
  ProdRule *red_rule;
} ActionInstr;

typedef struct _Action {
  ActionType act_type;
  ActionInstr act_instr;
} Action;


typedef struct _PTable {
  HashMap *action_t;
  HashMap *goto_t;
} PTable;


typedef struct _GotoParams {
  CC *cc;
  HashMap *goto_t;
} GotoParams;



typedef struct _ParseStack {
  char *sym;
  int state_no;
  struct _ParseStack *next;
} ParseStack;





int is_terminal(const char *name);
TokType terminal_name_to_type(const char *name);

int get_token(FILE *in, char *buf);
void unget_token(FILE *in, char *token);

void key_gen(char *buf, int state_no, const char *token_type);
PTable PTable_construct(const char *root, CC *cc, HashMap *gmap);
void add_to_goto(const char *key, void *val, void *params);
void act_table_free_el(const char *key, void *val, void *_);
void *state_list_reduce(const char *key, void *_, void *list);
void *non_terminals_list_reduce(const char *key, void *_, void *list);
void Action_print(Action act);
void PTable_print(PTable table);
void PTable_free(PTable table);


ParseStack *ParseStack_push(ParseStack *stack, const char *sym, int state_no);
ParseStack *ParseStack_pop(ParseStack *stack);
void ParseStack_free(ParseStack *stack);
void ParseStack_print(ParseStack *stack);


ProdRule *ProdRule_generate();
ProdRule *ProdRule_append(ProdRule *list, ProdRule *rule);
ProdRule *ProdRule_generate(const char *sym);
ProdRule *ProdRule_generate_list(FILE *g_file, const char *sym);
HashMap *gmap_generate(FILE *g_file, char *root_out);
void ProdRule_print(ProdRule *rule);
void ProdRule_print_list(ProdRule *list);
void gmap_print(HashMap *gmap);

void ProdRule_free(ProdRule *rule);
void ProdRule_free_list(ProdRule *prod_l);
void gmap_free(HashMap *gmap);


FirstSetEl *fmap_get(HashMap *fmap, const char *name);
HashMap *fmap_generate(HashMap *gmap, const char *root);
void fmap_generate_here(HashMap *map, const char *name, HashMap *gmap);
void fmap_print(HashMap *fmap);
void fmap_free(HashMap *fmap);


FirstSetEl *fset_insert(FirstSetEl *start, TokType t);
void fset_free(const char *_1, void *val, void *_2);

int cc_set_contains(LR1El *set, const char *sym, char *sym_l[],
      int pos, TokType lookahead, int num_rule_items);

int cc_set_equal(LR1El *a, LR1El *b);
int cc_set_is_subset(LR1El *set, LR1El *potential_subset);

LR1El *cc_set_append(LR1El *set, const char *sym, char *sym_l[],
      int pos, TokType lookahead, int num_rule_items);

void cc_set_free(LR1El *set);

void cc_set_print(LR1El *set);


CCStack *CCStack_push(CCStack *stack, CC *cc);
CCStack *CCStack_pop(CCStack *stack, CC *out);
void CCStack_free(CCStack *stack);


LR1El *closure_set(LR1El *set, HashMap *fmap, HashMap *gmap);
LR1El *goto_set(LR1El *set, const char *sym, HashMap *fmap, HashMap *gmap);

CC *CC_insert(CC *cc, int state_no, LR1El *cc_set);
CC *CC_find(CC *cc, LR1El *cc_set);
void CC_deconstruct(CC *root);
CC *CC_construct(HashMap *gmap, HashMap *fmap, const char *root);
void CC_print(CC *cc);


#endif
