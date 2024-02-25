%option noyywrap

%{
#include "parser.h"
%}


%%
[+] { return T_PLUS; }

[-] { return T_MINUS; }

[*] { return T_TIMES; }

[(] { return T_LBRACKET; }

[)] { return T_RBRACKET; }

[0-9]+([.][0-9]*)? { return T_NUMBER; }


.      /* ignore any other characters */

%%

/* Error handler. This will get called if none of the rules match. */
void yyerror (char const *s)
{
  fprintf (stderr, "Flex Error: %s\n", s); /* s is the text that wasn't matched */
  exit(1);
}
