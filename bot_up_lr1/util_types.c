#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "util_types.h"
#include "parse_types.h"

static unsigned multiplicative_string_hash_generic(const char *key, int coeff, int capacity);
static unsigned multiplicative_string_hash(const char *key, int capacity);
static unsigned multiplicative_string_rehash(const char *key, int capacity);


static void conn_print(const char *key, void *val);

void error(char *fmt, ...)
{
  char msg[ERR_MSG_LEN];
  va_list ap;
  va_start(ap, fmt),
  vsnprintf(msg, ERR_MSG_LEN, fmt, ap);
  va_end(ap);

  fprintf(stderr, "error: %s\n", msg);
  exit(1);
}

/******************************************************************************/
/* Generic linked list                                                        */
/******************************************************************************/

List *List_insert(List *list, void *val)
{
  List *new = malloc(sizeof(List));
  new->val = val;
  new->next = list;
  return new;
}

int List_contains(List *list, void *val, int (*comp_fn)(void *, void *))
{
  for (; list != NULL; list = list->next) {
    if ((comp_fn != NULL && comp_fn(list->val, val)) || list->val == val) {
      return 1;
    }
  }
  return 0;
}

void List_free(List *list, int free_val)
{
  List *next;
  for (; list != NULL; list = next) {
    next = list->next;
    if (free_val)
      free(list->val);
    free(list);
  }
}


/******************************************************************************/
/* Generic hash map                                                           */
/* Keys are strings and values can be anything from simple integers to        */
/* complicated structs                                                        */
/******************************************************************************/

HashMap *HashMap_construct(int val_size)
{
  HashMap *out;

  out = malloc(sizeof(HashMap));
  out->elts = calloc(INIT_CAP, sizeof(HashMapEl));
  out->capacity = INIT_CAP;

  out->val_size = val_size;
  out->size = 0;
  out->stack_base = calloc(out->capacity, val_size);

  return out;
}

// double the capacity of the map
void HashMap_expand(HashMap *map)
{
  void *old_stack;
  HashMapEl *old_elts;

  old_elts = map->elts;
  map->elts = calloc(2 * map->capacity, sizeof(HashMapEl));
  old_stack = map->stack_base;
  map->stack_base = calloc(2 * map->capacity, map->val_size);
  map->capacity *= 2;
  map->size = 0;

  // need to adjust all keys to their new hashed index
  for (int i = 0; i < map->capacity/2; i++) {
    if (old_elts[i].is_used) {
      HashMap_set(map, old_elts[i].key, old_elts[i].val);
      free(old_elts[i].key);
    }
  }
  free(old_stack);
  free(old_elts);
}

// store the data stored at the key 'key' in the memory area pointed to by 'out'
// if 'out' is NULL just return whether item exists in map or not
void *HashMap_get(HashMap *map, const char *key, unsigned *idx_out)
{
  unsigned first_idx, idx;

  idx = multiplicative_string_hash(key, map->capacity);

  if (!(map->elts[idx].is_used)) {
    if (idx_out != NULL)
      *idx_out = idx;
    return NULL;
  } else {
    if (strcmp(map->elts[idx].key, key) != 0) {
      first_idx = idx;

      do {
        idx = (idx + multiplicative_string_rehash(key, map->capacity))
          % map->capacity;

        if (map->elts[idx].is_used == 0) {
          if (idx_out != NULL)
            *idx_out = idx;
          return NULL;
        }
      } while (strcmp(map->elts[idx].key, key) != 0);
    }

    if (idx_out != NULL)
      *idx_out = idx;
    return map->elts[idx].val;
  }
}

void HashMap_set(HashMap *map, const char *key, void *val)
{
  unsigned idx;

  if ((HashMap_get(map, key, &idx)) == NULL) { // not found
    map->size++;
    map->elts[idx].is_used = 1;
    map->elts[idx].key = strdup(key);
    map->elts[idx].val = map->stack_base + map->size * map->val_size;
  }
  memcpy(map->elts[idx].val, val, map->val_size);
  // LOAD_FACTOR is in percent
  if (map->size >=  (LOAD_FACTOR * map->capacity)/100) {
    HashMap_expand(map);
  }
}

// execute given function on every value
void HashMap_iter(
    HashMap *map, void (*fn)(const char *, void *, void *), void *arg
)
{
  for (int i = 0; i < map->capacity; i++) {
    if (map->elts[i].is_used) {
      fn(map->elts[i].key, map->elts[i].val, arg);
    }
  }
}

void *HashMap_reduce(HashMap *map, void *(*fn)(const char *, void *, void *))
{
  void *out = NULL;
  for (int i = 0; i < map->capacity; i++) {
    if (map->elts[i].is_used) {
      out = fn(map->elts[i].key, map->elts[i].val, out);
    }
  }
  return out;
}

void HashMap_deconstruct(HashMap *map)
{
  for (int i = 0; i < map->capacity; i++) {
    if (map->elts[i].is_used) {
      free(map->elts[i].key);
    }
  }
  free(map->elts);
  free(map->stack_base);
  free(map);
}



static unsigned multiplicative_string_hash_generic(const char *key, int coeff, int capacity)
{
  unsigned hashval;


  for (hashval = 0; *key != '\0'; key++) {
    hashval = coeff * hashval + *key;
  }

  return hashval % capacity;
}

static unsigned multiplicative_string_hash(const char *key, int capacity)
{
  return multiplicative_string_hash_generic(key, COEFF1, capacity);
}

// return uneven number so that it will be relatively prime to map size which
// is power of 2
static unsigned multiplicative_string_rehash(const char *key, int capacity)
{
  unsigned hashval = multiplicative_string_hash_generic(key, COEFF2, capacity);
  return (hashval % 2 == 0) ? hashval + 1 : hashval;
}

int str_equal(void *a, void *b)
{
  if ((a == NULL && b != NULL) || (a != NULL && b == NULL))
    return 0;
  return (strcmp((char *)a, (char *)b) == 0);
}


void *str_copy(void *s)
{
  return (void *)strdup((char *)s);
}


/******************************************************************************/
/* String to integer map interface using hash map from above                  */
/* this is not used anywhere in the code and was used to test the hash map    */
/******************************************************************************/

HashMap *si_map_init()
{
  HashMap *out;

  out = HashMap_construct(sizeof(long));

  return out;
}

void si_map_set(HashMap *map, char *key, long val)
{
  HashMap_set(map, key, (void*)&val);
}


long si_map_get(HashMap *map, char *key)
{
  long *out;

  if ((out = (long *)HashMap_get(map, key, NULL)) == NULL) {
    return SI_DEFAULT;
  }
  return *out;
}

void si_map_test()
{
  HashMap *si_map = si_map_init();
  long val;
  char str_key[10];
  int passed = 0;
  for (int i = 0; i < 1000; i++) {
    snprintf(str_key, 10, "%d", i);
    si_map_set(si_map, str_key, (long)i);
    val = si_map_get(si_map, str_key);
    if (val != (long)i) {
      printf("Failed: Got %ld, Want: %d\n", val, i);
    } else {
      passed++;
    }
  }
  if (si_map_get(si_map, "random") == 0)
    printf("correct NULL result\n");
  printf("Passed %d/1000 tests\n", passed);

  HashMap_deconstruct(si_map);
}




/******************************************************************************/
/* Helper functions for treating array of strings as set                      */
/******************************************************************************/

void string_set_copy(char *s[], char *out[], int len)
{
  int i;

  for (i = 0; i < len; i++) {
    out[i] = strdup(s[i]);
  }
}

int string_set_equal(char *a[], char *b[], int len_a, int len_b)
{
  int i;

  if (len_a != len_b)
    return 0;

  for (i = 0; i < len_a; i++) {
    if (strcmp(a[i], b[i]) != 0)
      return 0;
  }


  return 1;
}
