#include <stdio.h>
#include <stdlib.h> // for exit
#include <string.h>

#include "parser.h"
#include "parse_types.h"
#include "util_types.h"


int check_grammar(PTable ptable, const char *root)
{
  ParseStack *pstack = ParseStack_push(NULL, root, 0);
  TokType tt = yylex();
  Action *act;
  int *goto_state;
  char buf[FORMAT_BUFLEN];
  int out = 0;

  while (1) {
    key_gen(buf, pstack->state_no, terminals[tt]);
    act = (Action *)HashMap_get(ptable.action_t, buf, NULL);
    if (act == NULL) {
      break;
    } else if (act->act_type == REDUCE) {
      for (int i = 0; i < act->act_instr.red_rule->num_symbols; i++) {
        pstack = ParseStack_pop(pstack);
      }
      key_gen(buf, pstack->state_no, act->act_instr.red_rule->sym);
      if ((goto_state = (int *)HashMap_get(ptable.goto_t, buf, NULL)) == NULL) {
        error("state %d needs to have a goto state for symbol '%s'",
            pstack->state_no, act->act_instr.red_rule->sym);
      }
      pstack = ParseStack_push(pstack, act->act_instr.red_rule->sym, *goto_state);
    } else if (act->act_type == SHIFT) {
      pstack = ParseStack_push(pstack, terminals[tt], act->act_instr.state_no);
      tt = yylex();
    } else if (act->act_type == ACCEPT && tt == NONE) {
      out = 1;
      break;
    } else {
      break;
    }
  }
  ParseStack_free(pstack);
  return out;
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: parser grammar_file [parse_file]\n");
    exit(1);
  }

  FILE *grammar_f;
  if (!(grammar_f = fopen(argv[1], "r"))) {
    error("can't open file '%s'", argv[1]);
  }

  char root[TOK_LEN];
  
  HashMap *gmap = gmap_generate(grammar_f, root);
  gmap_print(gmap);
  fclose(grammar_f);

  HashMap *fmap = fmap_generate(gmap, root);


  CC *cc = CC_construct(gmap, fmap, root);

  PTable ptable = PTable_construct(root, cc, gmap);

  if (check_grammar(ptable, root)) {
    printf("Grammar correct\n");
  } else {
    printf("Grammar incorrect\n");
  }

  CC_deconstruct(cc);
  PTable_free(ptable);
  fmap_free(fmap);
  gmap_free(gmap);
}



