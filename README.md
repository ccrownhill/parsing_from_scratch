# Experiments in implementing parsers

## Top down parser

(In `top_down` directory)

This implements a simple top-down algorithm to generate a parse tree given a
grammar in the file `grammar` (must be same directory as `parser` executable)
and some input from `stdin`.

Note that since this is a top down parser it can't handle left-associative/left-recursive
grammar.

See an example of how to describe mathematical expressions using this in `top_down/grammar`.

### Overview of how the program works

* It generates a symbol map to encode the grammar where every non-terminal
symbol is a key and every entry contains a list of production rules
* A production rule is just a linked list of symbols
* These data structures are used to parse the given input using a stack for
next non-terminals or terminals to check the tree against
* It is top down because it descends downwards into the grammar starting from
the root non-terminal (first one in grammar)

### Structure of the grammar file

Similar to Backus-Naur-Form.
Note that

* `:` have to follow a non-terminal immediately without spaces.
* an empty production rule stands for an $\epsilon$, i.e. a regex that matches
empty text

Example for modelling mathematical expressions in a right-recursive way:

```
expr: term expr'
expr': + term expr'
      | - term expr'
      |
term: factor term'
term': * factor term'
      | 
factor: ( expr )
      | n
```

## Naive bottom up parser (FAILED)

Using this kind of algorithm:

* use a stack which is empty at the beginning
* have a word which is the lookahead of the first symbol beyond the stack
* loop until top of stack is non-terminal symbol $S$ which is the target and EOF
is reached
  * if a production rule $p$ with $n$ elements matches the top $n$ elements of
  the stack
    * delete these elements from the stack
    * push the non-terminal on the LHS of $p$
  * else if word is not EOF
    * push the word onto the stack and get a new word
  * otherwise we have encountered a syntax error because we reached EOF and
  can't match the last symbols to a production rule even though we have not
  reached $S$ which would show that we are done
* if this point is reached the parse was **successful**

Note that I am using this grammar:

```
expr: expr T_PLUS term
    | expr T_MINUS term
    | term

term: term T_TIMES factor
    | factor

factor: T_LBRACKET expr T_RBRACKET
      | T_NUMBER
```

**Problem**:

Just choosing whichever handle works fails in examples like $3*4 + 3$
because it will reduce the first $3$ up to `expr` which means it can't be used
for reducing $3 * 4$ to `term`.

The reason for this is that the way to choose a handle is just to choose the
first production rule that fits.

**What needs to be done**

A more sophisticated handle choosing algorithm is needed.
In the following I detail my implementation of $LR(1)$ parsing that fixes this
problem while also being a lot more efficient (currently we have to loop over
all production rules every time when we look for a matching handle).

## Fix it with bottom up LR(1) parser

Note that many of the algorithms come from this textbook by Cooper and Torczon: Engineering a Compiler Chapter 3 "Parsers".

### Grammar

Note that for bottom up parsing the same grammar file was used but an aditional
`%start non_terminal` would be required to specify which non-terminal is the root:

```
%start expr

expr: expr T_PLUS term
    | expr T_MINUS term
    | term

term: term T_TIMES factor
    | factor

factor: T_LBRACKET expr T_RBRACKET
      | T_NUMBER
```

Note that this is left recursive grammar which a bottom up parser should be able
to handle.



### Why the name

* ***L***eft to right: parses input left to right
* ***R***ightmost derivation: this means that it will always try to expand the
right non-terminals of a production rule first. This is the reason why it **can**
handle left recursive grammar (i.e. left associative)
* ***1*** symbol lookahead

### Overview of how the LR(1) parser code works

For every `.c` file there is a corresponding `.h` file containing all the `struct`
and function declarations.

* `parser.c`: only `main` that calls all the data type construction functions
and `grammar_check` which uses the parse table to check whether the input
from `stdin` conforms to the grammar in the file specified as the first command
line argument
* `parse_types.c` contains several hash map types which are all interfaces to
`HashMap` from `util_types.c` or set types which are implemented using linked
lists:
  * `PTable` a struct containing a hash map for the action and the goto table (see below)
	* Grammar map: maps from non-terminal string to a linked list of production
  rules
  * First map: maps from string of token type to set of terminals that could
  come at start of given token type (e.g. `expr` could start with `(`)
  * `CC`: canonical collection of sets where every set represents one of the
  states in which the parser could be contains
    * `LR1El` structs
    that contain a rule, a position within the rule, and a lookahead symbol
    that could come after the rule (used to indicate when to reduce)
    * a `HashMap` that maps from token types to a next set that the parser
    would transition to if the input where this token type
* `util_types.c`: Most important: `HashMap` which is a more or less generic hash
map
  * keys are strings
  * values are `void *` pointers which can be cast to any type
  * hash functions map from strings to indexes in array with set capacity
  * using open addressing, i.e. one element in each array element and rehashing
  upon hash collisions
  * both hash and rehash functions use multiplicative hashing
  * when initializing you specify how big the type is to which your pointer
  values point to (could be `int` or some `struct` with multiple fields)
  * the map then allocates a stack as a contiguous block of memory to which it
  will `memcpy` the specified amount of bytes (during list initialization)
  from the given value pointer when setting elements.
  * Doubles in size when it is 75% filled
  * capacity is always power of 2 and rehash function always returns odd value
  to make sure that repeatedly adding the value of the rehash function to the
  original hash value will eventually cover all elements of the underlying array


### What the LR(1) parse table looks like and how to use it

**In the code**:

* `PTable` in `parse_types.c`/`parse_types.h`

The LR(1) parse table tells us when to reduce and when to keep shifting the next
token from the scanner onto a stack.

It consists of an action table and a goto table.

Both have the following columns and rows:

* *rows* for every state (i.e. a set in the canonical collection of sets)
* *columns* for all token types

**Action table**

In row `i` and column with lookahead symbol `s` you can find out whether you

* *shift*, i.e. read another element from the input and push it onto the stack (the
state you go to after this is also in that entry in the action table)
* or `reduce`, i.e. apply a given production rule (specified in the action table
entry)

**Goto table**

Tells you what state you go to after you reduced where the column is the symbol
you reduced to and the row is given by the state you where in before reading in
the symbols that are now reduced.

**How to use it for parsing in `check_grammar` (in parser.c)**

Note that we use a stack which contains pairs of

* terminal or non-terminal symbol
* state number

This will be used like this:

* initially, we push `<Goal, 0>` on the stack
* if we encounter a terminal symbol (coming from the scanner) we look up the action
in the *Action* table at our current state's row
* if it is a shift `s N` we push the scanner token onto the stack along with the state number
and go to state `N`
* if it is a reduce `r N` we pop the items that were reduced from the stack
and go to the state specified by `Goto[top state on stack, LHS of reduce]` where
`top state on stack` is the state we were in before the non-terminal to which
we have reduced the top of the stack and `LHS of reduce` is that non-terminal to
which we have reduced


### Basic principle of the table construction, LR(1) items, and the canonical collection of sets

**In code**:

`PTable_construct(const char *root, CC *cc, HashMap *gmap)` in `parse_types.c`

where

* `root` is the root non-terminal of the grammar
* `cc` is the canonical collection of sets and `gmap` the grammar map mapping
non-terminals to lists of production rules.

To construct a table we need to find the *canonical collection of sets of LR(1) items* which is a bunch of sets
containing *LR(1) items*, i.e.

* a production rule $A \leftarrow \beta \gamma$
* a position within the production rule represented by $\bullet$: $A \leftarrow \beta \bullet \gamma$
* a lookahead symbol $a$ (has to be a terminal) that can come at the end of that rule: $[A \leftarrow \beta \bullet \gamma, a]$.
This is used to determine whether we have a handle $<A \leftarrow \beta \gamma, a>$
that we can reduce upon receiving input $a$.
This is the case when the bullet is at the end of the production rule: $[A \leftarrow \beta \gamma \bullet, a]$.

Every set $CC_i$ in the *canonical collection* contains all the possible
*LR(1) items* that can occur in a state.

In our example we have the following sets:

The sets are connected by connections specifying a terminal symbol that would
take the parser from one state to the next.

These transitions connections are defined in this table:


To construct a table from these sets we employ the following technique:

Note that all $goto(CC_i, n)$ statements will be computed using the transition
connection table.

**Fill the action table**

Go over all sets $CC_i$ in the canonical collection $CC$:

Go over all items $I$ in $CC_i$:

* if the bullet is at the end (means we can reduce if lookahead matches input)
  * add reduce action $rN$ at row $i$ in column of lookahead $a$ of item in *Action table*
  * set $N$ to number of production rule that $I$ represents
* if the bullet is before terminal symbol $c$ (means we shift this item onto the stack and advance to the next state)
  * set row $i$ and column $c$ of *Action table* to shift $sN$ where $N$ is the state number of $goto(CC_i, c)$
* if $I = [Goal \leftarrow \beta \bullet, eof]$
  * set row $i$ column $eof$ to $"accept"$

**Filling the Goto table**

Note that since this also loops over all sets in the *canonical collection* the
code should be placed in the same loop as the one for filling the *Action table*.

Go over all sets $CC_i$ in the canonical collection $CC$:

* Loop over all non-terminals $nt$ in the symbol list
* set *Goto table* at row $i$ and column $nt$ to the state number corresponding
to the set returned by $goto(CC_i, nt)$

### Helper function for constructing the canonical collection

To construct the canonical collection of sets we need the following two functions:

* $closure(s)$ given set $s$ of *LR(1) items* extend set by all possible production
rules that refer to the same state. E.g. If you have the *LR(1) item*
$[Goal \leftarrow \bullet List, eof]$ this is equivalent to expanding the production
rules of $List$ with $\bullet$ at their beginning, i.e. $[List \leftarrow \bullet Pair, eof]$,
$[List \leftarrow \bullet Pair, (]$, etc.

* $goto(s, x)$: given current set $s$ and input terminal/non-terminal return
closure referring to next state after $x$ has been encountered

**closure(s) implementation `closure_set` (in parse_types.c)**

loop over all items $[A \leftarrow \beta \bullet C \delta, a]$ of set $s$:
* find all productions with $C$ on the LHS
  * for every possible symbol that could follow them in this case, i.e. the
  $FIRST(\delta)$ symbol using the definition of $FIRST$ from above.
  (here it says $FIRST(\delta \\ a)$ but I think that this is wrong since it is
  also not compatible with the way $FIRST$ is defined since it expects just one
  terminal/non-terminal symbol
    * add to $s$ $[C \leftarrow \bullet\ "rest\ of\ C\ rule",\ FIRST(\delta)]$
* done now

**goto(s, x) implementation `goto_set` (in parse_types.c)**

* find all LR(1) items in $s$ which have a $\bullet$ before $x$
* add them to the resulting set with the $\bullet$ after $x$
* compute the closure of these sets to make sure to cover every possible
LR(1) item that could be in this set

### Construct the canonical collection of sets

**In code**:

* `CC` is defined as a `struct` in `parse_type.h`
* it is a linked list sorted by `state_no`
* every item in `CC` contains a hash map that maps from input token types
to another set
* the algorithm below is implemented in `CC_construct` in `parse_types.c`

Here I am describing an implementation using a $worklist$ of sets from which
to explore connections while the implementation from the book uses a way to
mark sets as covered:

* Let initial set $CC_0$ be the closure for all the first state production rules
followed by $eof$ with the $\bullet$ at the beginning:

$$closure({[Goal \leftarrow \bullet List, eof], [Goal \leftarrow other\ production\ rule\ for\ Goal\ if\ any,\ eof]})$$

* add $CC_0$ to $worklist$
* set $statenum\ \leftarrow \ 1$
* while the $worklist$ is not empty
  * $workset\ \leftarrow\ pop\ top\ of\ worklist$
  * for every $x$ which is preceded by a $\bullet$ in any of the $LR(1) items$ of $workset$
    * $tmpset\ \leftarrow\ goto(workset,\ x)$
    * if tmpset not in $CC$
      * add it to $CC$
      * add tmpset to $workset$
    * in transition hash map value at key key $x$ to the state number $statenum$ of $tmpset$
    * $statenum\ \leftarrow\ statenum\ +\ 1$
