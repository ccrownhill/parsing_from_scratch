#ifndef UTIL_TYPES_H
#define UTIL_TYPES_H

#define FORMAT_BUFLEN (TOK_LEN + 5)

#define SI_DEFAULT 0

// coefficients for multiplicative hashing
#define COEFF1 31
#define COEFF2 37

#define INIT_CAP 64
#define LOAD_FACTOR 75 // in percent of map capacity
#define ERR_MSG_LEN 200

typedef struct _HashMap {
  struct _HashMapEl *elts;
  int val_size;
  int capacity;
  int size;
  void *stack_base;
} HashMap;

typedef struct _HashMapEl {
  char *key;
  void *val;
  int is_used;
} HashMapEl;

typedef struct _List {
  void *val;
  struct _List *next;
} List;

void error(char *fmt, ...);

List *List_insert(List *list, void *val);
int List_contains(List *list, void *val, int (*comp_fn)(void *, void *));
void List_free(List *list, int free_val);

void HashMap_expand(HashMap *map);
void *HashMap_get(HashMap *map, const char *key, unsigned *idx_out);
void HashMap_set(HashMap *map, const char *key, void *val);

HashMap *HashMap_construct(int val_size);

void HashMap_iter(
    HashMap *map, void (*fn)(const char *, void *, void *), void *arg);

void *HashMap_reduce(HashMap *map, void *(*fn)(const char *, void *, void*));
void HashMap_deconstruct(HashMap *map);

int str_equal(void *a, void *b);
void *str_copy(void *s);

void string_set_copy(char *s[], char *out[], int len);
int string_set_equal(char *a[], char *b[], int len_a, int len_b);

HashMap *si_map_init();
void si_map_set(HashMap *map, char *key, long val);
long si_map_get(HashMap *map, char *key);
void si_map_test();

#endif
