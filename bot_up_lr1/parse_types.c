#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "parse_types.h"
#include "util_types.h"

static void gmap_free_val(const char *_1, void *val, void *_2);
static void gmap_print_el(const char *key, void *val, void *_);
static void conn_print(const char *key, void *val, void *_);

char *terminals[] = {"NONE", "T_PLUS", "T_MINUS", "T_TIMES", "T_LBRACKET", "T_RBRACKET", "T_NUMBER"};
//char *terminals[] = {"NONE", "T_LBRACKET", "T_RBRACKET"};

int is_terminal(const char *name)
{
  for (int i = 0; i < NUM_TERMINALS; i++) {
    if (strcmp(name, terminals[i]) == 0)
      return 1;
  }
  return 0;
}

TokType terminal_name_to_type(const char *name)
{
  for (int i = 0; i < NUM_TERMINALS; i++) {
    if (strcmp(terminals[i], name) == 0) {
      return (TokType)i;
    }
  }
  return NONE;
}


/******************************************************************************/
/* Parse table                                                                */
/******************************************************************************/

void key_gen(char *buf, int state_no, const char *token_type)
{
  snprintf(buf, FORMAT_BUFLEN, "%d %s", state_no, token_type);
}

PTable PTable_construct(const char *root, CC *cc, HashMap *gmap)
{
  LR1El *set_iter;
  Action act;
  PTable out;
  GotoParams gparams;
  char buf[FORMAT_BUFLEN];

  out.action_t = HashMap_construct(sizeof(Action));
  out.goto_t = HashMap_construct(sizeof(int));


  for (; cc != NULL; cc = cc->next) {
    for (set_iter = cc->cc_set; set_iter != NULL; set_iter = set_iter->next) {
      if (strcmp(set_iter->rule->sym, root) == 0 &&
          set_iter->pos == set_iter->rule->num_symbols &&
          set_iter->lookahead == NONE) {
        act.act_type = ACCEPT;      
        act.act_instr.state_no = 0; // this is a don't care because there will be no
                                 // transition to another state after accepting

        key_gen(buf, cc->state_no, terminals[NONE]);

        HashMap_set(out.action_t, buf, (void *)&act);
      } else if (set_iter->pos == set_iter->rule->num_symbols) {
        act.act_type = REDUCE;
        act.act_instr.red_rule = set_iter->rule;

        key_gen(buf, cc->state_no, terminals[set_iter->lookahead]);

        HashMap_set(out.action_t, buf, (void *)&act);
      } else if (is_terminal(set_iter->rule->sym_l[set_iter->pos])) {
        act.act_type = SHIFT;
        CC **cc_next =
          (CC **)HashMap_get(
            cc->goto_map,
            set_iter->rule->sym_l[set_iter->pos],
            NULL
          );

        act.act_instr.state_no = (*cc_next)->state_no;

        key_gen(buf, cc->state_no, set_iter->rule->sym_l[set_iter->pos]);
        HashMap_set(out.action_t, buf, (void *)&act);
      }
    }

    gparams.cc = cc;
    gparams.goto_t = out.goto_t;
    HashMap_iter(gmap, add_to_goto, (void *)&gparams);
  }

  return out;
}


void add_to_goto(const char *key, void *val, void *params)
{
  GotoParams *p = (GotoParams *)params;
  CC **next_cc;
  char buf[FORMAT_BUFLEN];

  if (!(is_terminal(key))) {
    if ((next_cc = (CC **)HashMap_get(p->cc->goto_map, key, NULL)) == NULL) {
      return;
    }

    key_gen(buf, p->cc->state_no, key);

    HashMap_set(p->goto_t, buf, (void *)&((*next_cc)->state_no));
  }
}

void *state_list_reduce(const char *key, void *_, void *list)
{
  char *state_loc = strstr(key, " ");
  *state_loc = '\0';
  long state_no = (long)atoi(key);
  *state_loc = ' ';
  if (List_contains(list, (void *)state_no, NULL))
    return list;
  else
    return List_insert(list, (void *)state_no);
}

void *non_terminals_list_reduce(const char *key, void *_, void *list)
{
  char *nt_loc = strstr(key, " ") + 1;
  if (List_contains(list, (void *)nt_loc, str_equal))
    return list;
  else
    return List_insert(list, (void *)strdup(nt_loc));
}

void Action_print(Action act)
{
  switch (act.act_type) {
    case SHIFT:
      printf("s %d", act.act_instr.state_no);
      break;
    case REDUCE:
      printf("r [");
      ProdRule_print(act.act_instr.red_rule);
      printf("]");
      break;
    case ACCEPT:
      printf("acc");
      break;
  }
}

void PTable_print(PTable table)
{
  char buf[FORMAT_BUFLEN];
  Action *act;
  List *iter1, *iter2;
  printf("Action table:\n");

  long state;
  List *states = (List *)HashMap_reduce(table.action_t, state_list_reduce);
  for (iter1 = states; iter1 != NULL; iter1 = iter1->next) {
    state = (long)iter1->val; 
    printf("Row for state %ld\n", state);
    // loop over all columns which are given by the non-terminals
    for (int i = 0; i < NUM_TERMINALS; i++) {
      printf("%s=", terminals[i]);
      key_gen(buf, state, terminals[i]);
      if ((act = (Action *)HashMap_get(table.action_t, buf, NULL)) == NULL) {
        printf("empty");
      } else {
        Action_print(*act);
      }
      printf("; ");
    }
    printf("\n");
  }

  printf("---------------------\n");
  printf("Goto table:\n");

  List *non_terminals =
    (List *)HashMap_reduce(table.goto_t, non_terminals_list_reduce);

  int *goto_state;
  for (iter1 = states; iter1 != NULL; iter1 = iter1->next) {
    state = (long)iter1->val;
    printf("Row for state %ld\n", state);
    for (iter2 = non_terminals; iter2 != NULL; iter2 = iter2->next) {
      key_gen(buf, state, (char *)iter2->val);
      printf("%s=", (char *)iter2->val);
      if ((goto_state = (int *)HashMap_get(table.goto_t, buf, NULL)) == NULL) {
        printf("empty");
      } else {
        printf("%d", *goto_state);
      }
      printf("; ");
    }
    printf("\n");
  }

  List_free(states, 0);
  List_free(non_terminals, 1);
}

void PTable_free(PTable table)
{
  HashMap_deconstruct(table.action_t);
  HashMap_deconstruct(table.goto_t);
}


ParseStack *ParseStack_push(ParseStack *stack, const char *sym, int state_no)
{
  ParseStack *new = malloc(sizeof(ParseStack));
  new->sym = strdup(sym);
  new->state_no = state_no;
  new->next = stack;
  return new;
}

ParseStack *ParseStack_pop(ParseStack *stack)
{
  if (stack == NULL)
    return NULL;
  ParseStack *out = stack->next;
  free(stack->sym);
  free(stack);
  return out;
}

void ParseStack_free(ParseStack *stack)
{
  while ((stack = ParseStack_pop(stack)) != NULL);
}

void ParseStack_print(ParseStack *stack)
{
  printf("Stack contents...\n");
  for (; stack != NULL; stack = stack->next) {
    printf("<%s, %d>; ", stack->sym, stack->state_no);
  }
  printf("\n");
}


/******************************************************************************/
/* Grammar map                                                                */
/******************************************************************************/

static void gmap_print_el(const char *key, void *val, void *_)
{
  printf("Non-terminal: '%s'\n", key);
  ProdRule_print_list(*((ProdRule **)val));
}

void gmap_print(HashMap *gmap)
{
  HashMap_iter(gmap, gmap_print_el, NULL);
}

void ProdRule_print(ProdRule *rule)
{
  int i;
  printf("[%s <- ", rule->sym);
  for (i = 0; i < rule->num_symbols; i++) {
    printf(" '%s' ", rule->sym_l[i]);
  }
  printf("]");
}

void ProdRule_print_list(ProdRule *list)
{
  ProdRule *rule_iter;

  for (rule_iter = list; rule_iter != NULL; rule_iter = rule_iter->next) {
    ProdRule_print(rule_iter);
    printf("\n");
  }
}

ProdRule *ProdRule_append(ProdRule *list, ProdRule *rule)
{
  ProdRule *last;

  APPEND(list, last, rule);
}

ProdRule *ProdRule_generate(const char *sym)
{
  ProdRule *out = malloc(sizeof(ProdRule));
  out->sym = strdup(sym);
  out->num_symbols = 0;
  out->next = NULL;
  return out;
}


ProdRule *ProdRule_generate_list(FILE *g_file, const char *sym)
{
  char token[TOK_LEN];
  ProdRule *tmp_rule;
  
  ProdRule *prod_l;

  prod_l = NULL;
  tmp_rule = ProdRule_generate(sym);

  while (get_token(g_file, token) != EOF && token[strlen(token)-1] != ':') {
    if (token[0] != '|') {
      if (tmp_rule->num_symbols < MAX_TERMS_PER_RULE) {
        tmp_rule->sym_l[tmp_rule->num_symbols++] = strdup(token);
      } else {
        error("Grammar parsing: exceeded maximum number of symbols"
            "per production rule. "
            "Can't be over '%d'", MAX_TERMS_PER_RULE);
      }
    } else {
      prod_l = ProdRule_append(prod_l, tmp_rule);
      tmp_rule = ProdRule_generate(sym);
    }
  }
  prod_l = ProdRule_append(prod_l, tmp_rule);
  if (token[strlen(token)-1] == ':')
    unget_token(g_file, token);

  return prod_l;
}



HashMap *gmap_generate(FILE *g_file, char *root_out)
{
  char token[TOK_LEN];
  ProdRule *prod_l;
  HashMap *gram_map;

  gram_map = HashMap_construct(sizeof(ProdRule *));


  if (get_token(g_file, token) == EOF) {
    error("grammar file empty");
  }
  if (strcmp(token, "%start") != 0) {
    error("expecting '%%start' to define root of grammar");
  }
  if (get_token(g_file, token) == EOF) {
    error("missing root of grammar name after '%%start'");
  }
  strncpy(root_out, token, TOK_LEN);


  while (get_token(g_file, token) != EOF) {
    if (token[strlen(token)-1] == ':') {
      token[strlen(token)-1] = '\0';
      prod_l = ProdRule_generate_list(g_file, token);
      HashMap_set(gram_map, token, (void *)&prod_l);
    } else {
      error("syntax: non-terminals must be defined with a"
          "':' without spaces separating it from the non-terminal name");
    }
  }
  return gram_map;
}

// assume that all tokens are separated by spaces
int get_token(FILE *in, char *buf)
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

void unget_token(FILE *in, char *token)
{
  if (ungetc(' ', in) == EOF) {
    error("couldn't unget character ' '");
  }
  for (int i = strlen(token) - 1; i >= 0; i--) {
    if (ungetc(token[i], in) == EOF) {
      error("couldn't unget character '%c'", token[i]);
    }
  }
}

void ProdRule_free(ProdRule *rule)
{
  int i;
  for (i = 0; i < rule->num_symbols; i++) {
    free(rule->sym_l[i]);
  }
  free(rule->sym);
  free(rule);
}

void ProdRule_free_list(ProdRule *prod_l)
{
  ProdRule *next;

  for (; prod_l != NULL; prod_l = next) {
    next = prod_l->next;
    ProdRule_free(prod_l);
  }
}

static void gmap_free_val(const char *_1, void *val, void *_2)
{
  ProdRule_free_list(*((ProdRule **)val));
}

void gmap_free(HashMap *gmap)
{
  HashMap_iter(gmap, gmap_free_val, NULL);
  HashMap_deconstruct(gmap);
}


/******************************************************************************/
/* First map																																	*/
/******************************************************************************/


FirstSetEl *fmap_get(HashMap *fmap, const char *name)
{
  FirstSetEl **out;
  if ((out = HashMap_get(fmap, name, NULL)) == NULL) {
    return NULL;
  }
  return *out;
}


HashMap *fmap_generate(HashMap *gmap, const char *root)
{
  HashMap *out;
  FirstSetEl *fset_el;

  out = HashMap_construct(sizeof(FirstSetEl *));

  for (int i = 0; i < NUM_TERMINALS; i++) {
    fset_el = fset_insert(NULL, i);
    HashMap_set(out, terminals[i], (void *)&fset_el);
  }

  fmap_generate_here(out, root, gmap);
  return out;
}

void fmap_generate_here(HashMap *fmap, const char *name, HashMap *gmap)
{
  ProdRule *prod_l, *rule_iter;
  FirstSetEl *fset, *set_iter, **ret;

  if ((HashMap_get(fmap, name, NULL)) == NULL) {
    // generate new first map element with empty items list
    fset = NULL;
    // assume name exists in map because we generate first sets only for
    // entries from grammar map
    prod_l = *((ProdRule **)HashMap_get(gmap, name, NULL));
    for (rule_iter = prod_l; rule_iter != NULL; rule_iter = rule_iter->next) {
      if (strcmp(name, rule_iter->sym_l[0]) == 0)
        continue;
      fmap_generate_here(fmap, rule_iter->sym_l[0], gmap);
      ret = (FirstSetEl **)HashMap_get(fmap, rule_iter->sym_l[0], NULL);

      for (set_iter = *ret; set_iter != NULL; set_iter = set_iter->next) {
        fset = fset_insert(fset, set_iter->token_type);
      }
    }
    HashMap_set(fmap, name, (void *)&fset);
  }
}

void fmap_print_el(const char *name, void *val, void *_)
{
  FirstSetEl *fset = *((FirstSetEl **)val);

  printf("FIRST(%s): {", (char *)name);
  for (; fset != NULL; fset = fset->next) {
    printf("%s,", terminals[fset->token_type]); 
  }
  printf("}\n");
}

void fmap_print(HashMap *fmap)
{
  HashMap_iter(fmap, fmap_print_el, NULL);
}

void fmap_free(HashMap *fmap)
{
  HashMap_iter(fmap, fset_free, NULL);
  HashMap_deconstruct(fmap);
}

/******************************************************************************/
/* First set                                                                  */
/******************************************************************************/


FirstSetEl *fset_insert(FirstSetEl *start, TokType t)
{
  FirstSetEl *new, *iter, *prev;

  prev = NULL;
  for (iter = start; iter != NULL; iter = iter->next) {
    if (t < iter->token_type) {
      new = malloc(sizeof(FirstSetEl));
      new->token_type = t;
      new->next = iter;
      if (prev != NULL) {
        prev->next = new;
        return start;
      } else {
        return new;
      }
    } else if (t == iter->token_type) { // don't add items that exist in set
                                        // already
      return start;
    }
    prev = iter;
  }

  // if it is not smaller than any other element insert it at the end
  new = malloc(sizeof(FirstSetEl)); // rewrite malloc because it might be
                                    // avoided in case when t is in set already
                                    // this way no unnecessary memory allocations
                                    // will be performed

  new->token_type = t;
  new->next = NULL;
  if (prev == NULL) {
    return new;
  } else {
    prev->next = new;
    return start;
  }
}



void fset_free(const char *_1, void *val, void *_2)
{
  FirstSetEl *next, *iter;
  for (iter = *((FirstSetEl **)val); iter != NULL; iter = next) {
    next = iter->next;
    free(iter);
  }
}

/******************************************************************************/
/* Canonical Collection Set																										*/
/******************************************************************************/

int cc_set_contains(LR1El *set, const char *sym, char *sym_l[],
                    int pos, TokType lookahead, int num_rule_items)
{
  LR1El *iter;

  for (iter = set; iter != NULL; iter = iter->next) {
    if (strcmp(iter->rule->sym, sym) == 0 &&
        string_set_equal(iter->rule->sym_l,
          sym_l, iter->rule->num_symbols, num_rule_items) &&
        iter->pos == pos && iter->lookahead == lookahead)
      return 1;
  }

  return 0;
}

int cc_set_is_subset(LR1El *set, LR1El *potential_subset)
{
  LR1El *iter;
  for (iter = potential_subset; iter != NULL; iter = iter->next) {
    if (!(cc_set_contains(set, iter->rule->sym, iter->rule->sym_l, iter->pos,
            iter->lookahead, iter->rule->num_symbols))) {
      return 0;
    }
  }
  return 1;
}

int cc_set_equal(LR1El *a, LR1El *b)
{
  return (cc_set_is_subset(a, b) && cc_set_is_subset(b, a));
}

LR1El *cc_set_append(LR1El *set, const char *sym, char *sym_l[],
                    int pos, TokType lookahead, int num_rule_items)
{
  if (cc_set_contains(set, sym, sym_l,
        pos, lookahead, num_rule_items)) {
    return set;
  }
  LR1El *new = malloc(sizeof(LR1El));
  new->rule = ProdRule_generate(sym);
  new->rule->num_symbols = num_rule_items;
  string_set_copy(sym_l, new->rule->sym_l, num_rule_items);
  new->pos = pos;
  new->lookahead = lookahead;
  new->next = NULL;

  LR1El *last;

  APPEND(set, last, new);
}

void cc_set_free(LR1El *set)
{
  LR1El *next;
  int i;

  for (; set != NULL; set = next) {
    next = set->next;
    ProdRule_free(set->rule);
    free(set);
  }
}

void cc_set_print(LR1El *set)
{
  int i;

  printf("{\n");
  for (; set != NULL; set = set->next) {
    printf("[%s -> ", set->rule->sym);
    for (i = 0; i < set->rule->num_symbols; i++) {
      if (set->pos == i)
        printf(" o");
      printf(" %s", set->rule->sym_l[i]);
    }
    if (set->pos == i)
      printf(" o");
    printf(", %s]\n", terminals[set->lookahead]);

  }
  printf("}\n");
}



// compute complete set of LR1 elements by trying to expand all of the
// LR1 elements in set at the parsing position to get further LR1 elements
LR1El *closure_set(LR1El *set, HashMap *fmap, HashMap *gmap)
{
  LR1El *iter;
  ProdRule *rule, **gmap_get_out;
  FirstSetEl *found;
  int lookahead, i;
  int counter = 0;


  for (iter = set; iter != NULL; iter = iter->next) {
    if (iter->pos < iter->rule->num_symbols &&
        (gmap_get_out =
         (ProdRule **)HashMap_get(gmap, iter->rule->sym_l[iter->pos], NULL)) != NULL) {
      rule = *gmap_get_out;
      for (; rule != NULL; rule = rule->next) {
        // go over all FIRST elements of
        if (iter->pos + 1 < iter->rule->num_symbols &&
            (found = fmap_get(fmap, iter->rule->sym_l[iter->pos + 1])) != NULL) {
          for (; found != NULL; found = found->next) {
            // insert into set
            // note that this function will not allow duplicates to be inserted
            set = cc_set_append(set, iter->rule->sym_l[iter->pos], rule->sym_l,
                  0, found->token_type, rule->num_symbols);
          }
        } else {
          set = cc_set_append(set, iter->rule->sym_l[iter->pos], rule->sym_l,
                0, iter->lookahead, rule->num_symbols);
        }
      }
    }
  }

  return set;
}

LR1El *goto_set(LR1El *set, const char *sym, HashMap *fmap, HashMap *gmap)
{
  LR1El *out = NULL;

  for (; set != NULL; set = set->next) {
    // if parsing position has already reached end of rule can't go anywhere
    if (set->pos + 1 <= set->rule->num_symbols &&
        // check if goto_set symbol follows current parsing position
        strcmp(set->rule->sym_l[set->pos], sym) == 0) {
      out = cc_set_append(out, set->rule->sym, set->rule->sym_l, set->pos + 1, set->lookahead,
          set->rule->num_symbols);
    }
  }

  return closure_set(out, fmap, gmap);
}


/******************************************************************************/
/* Canonical Collection of Sets																								*/
/******************************************************************************/


CC *CC_insert(CC *cc, int state_no, LR1El *cc_set)
{
  CC *out = malloc(sizeof(CC));
  out->state_no = state_no;
  out->cc_set = cc_set;

  out->goto_map = HashMap_construct(sizeof(CC *));

  out->next = cc;
  return out;
}

CC *CC_find(CC *cc, LR1El *cc_set)
{
  CC *iter;
  for (iter = cc; iter != NULL; iter = iter->next) {
    if (cc_set_equal(iter->cc_set, cc_set)) {
      return iter;
    }
  }
  return NULL;
}

void CC_deconstruct(CC *root)
{
  CC *next;
  for (; root != NULL; root = next) {
    next = root->next;
    HashMap_deconstruct(root->goto_map);
    cc_set_free(root->cc_set);
    free(root);
  }
}

CC *CC_construct(HashMap *gmap, HashMap *fmap, const char *root)
{
  CC *out, *workset, *goto_target;
  CCStack *workstack;  
  LR1El *cc_set, *iter_set;
  ProdRule *rule, **gmap_get_out;
  int state_no;

  out = NULL;
  workstack = NULL;
  cc_set = NULL; // empty set
  state_no = 0;
  workset = malloc(sizeof(CC));

  // get first item in grammar map
  if ((gmap_get_out = (ProdRule **)HashMap_get(gmap, root, NULL)) != NULL) {
    rule = *gmap_get_out;
    for (; rule != NULL; rule = rule->next) {
      cc_set = cc_set_append(cc_set, root, rule->sym_l, 0, NONE, rule->num_symbols);
    }
  }
  cc_set = closure_set(cc_set, fmap, gmap);
  out = CC_insert(out, state_no, cc_set);

  workstack = CCStack_push(workstack, out);
  while (workstack != NULL) {
    workstack = CCStack_pop(workstack, workset);
    for (iter_set = workset->cc_set; iter_set != NULL; iter_set = iter_set->next) {
      if (iter_set->pos < iter_set->rule->num_symbols) {
        cc_set = goto_set(workset->cc_set, iter_set->rule->sym_l[iter_set->pos], fmap, gmap);
        if ((goto_target = CC_find(out, cc_set)) == NULL) {
          out = CC_insert(out, ++state_no, cc_set);
          workstack = CCStack_push(workstack, out);
          goto_target = out;
        } else {
          cc_set_free(cc_set);
        }
        HashMap_set(workset->goto_map, iter_set->rule->sym_l[iter_set->pos],
            (void *)&goto_target);
      }
    }
  }

  free(workset);
  return out;
}

static void conn_print(const char *key, void *val, void *_)
{
  CC *conn_item = *((CC **) val);
  printf("if '%s' -> %d\n", key, conn_item->state_no);
}

void CC_print(CC *cc)
{
  for (; cc != NULL; cc = cc->next) {
    printf("State %d: ", cc->state_no);
    cc_set_print(cc->cc_set);
    printf("Connected to these states over these connections: [\n");
    HashMap_iter(cc->goto_map, conn_print, NULL);
    printf("]\n\n");
  }
}



CCStack *CCStack_push(CCStack *stack, CC *cc)
{
  CCStack *new = malloc(sizeof(CCStack));
  new->cc = cc;
  new->next = stack;
  return new;
}

CCStack *CCStack_pop(CCStack *stack, CC *out)
{
  memcpy(out, stack->cc, sizeof(CC));
  CCStack *new_first = stack->next;
  free(stack);
  return new_first;
}

void CCStack_free(CCStack *stack)
{
  CCStack *next;
  for (; stack != NULL; stack = next) {
    next = stack->next;
    free(stack);
  }
}
