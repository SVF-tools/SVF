/**CFile***********************************************************************

  FileName    [st.c]

  PackageName [st]

  Synopsis    [Symbol table package.]

  Description [The st library provides functions to create, maintain,
  and query symbol tables.]

  SeeAlso     []

  Author      []

  Copyright   []

******************************************************************************/

#include "CUDD/util.h"
#include "CUDD/st.h"

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] UTIL_UNUSED = " $Id: st.c,v 1.12 2010/04/22 19:00:55 fabio Exp fabio $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#define ST_NUMCMP(x,y) ((x) != (y))

#define ST_NUMHASH(x,size) ((unsigned long)(x)%(size))

#if SIZEOF_VOID_P == 8
#define st_shift 3
#else
#define st_shift 2
#endif

#define ST_PTRHASH(x,size) ((unsigned int)((unsigned long)(x)>>st_shift)%size)

#define EQUAL(func, x, y) \
    ((((func) == st_numcmp) || ((func) == st_ptrcmp)) ?\
      (ST_NUMCMP((x),(y)) == 0) : ((*func)((x), (y)) == 0))

#define do_hash(key, table)\
    ((int)((table->hash == st_ptrhash) ? ST_PTRHASH((char *)(key),(table)->num_bins) :\
     (table->hash == st_numhash) ? ST_NUMHASH((char *)(key), (table)->num_bins) :\
     (*table->hash)((char *)(key), (table)->num_bins)))

#define PTR_NOT_EQUAL(table, ptr, user_key)\
(ptr != NIL(st_table_entry) && !EQUAL(table->compare, (char *)user_key, (ptr)->key))

#define FIND_ENTRY(table, hash_val, key, ptr, last) \
    (last) = &(table)->bins[hash_val];\
    (ptr) = *(last);\
    while (PTR_NOT_EQUAL((table), (ptr), (key))) {\
	(last) = &(ptr)->next; (ptr) = *(last);\
    }\
    if ((ptr) != NIL(st_table_entry) && (table)->reorder_flag) {\
	*(last) = (ptr)->next;\
	(ptr)->next = (table)->bins[hash_val];\
	(table)->bins[hash_val] = (ptr);\
    }

/* This macro does not check if memory allocation fails. Use at you own risk */

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int rehash (st_table *);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Create and initialize a table.]

  Description [Create and initialize a table with the comparison function
  compare_fn and hash function hash_fn. compare_fn is
  <pre>
	int compare_fn(const char *key1, const char *key2)
  </pre>
  It returns <,=,> 0 depending on whether key1 <,=,> key2 by some measure.<p>
  hash_fn is
  <pre>
	int hash_fn(char *key, int modulus)
  </pre>
  It returns a integer between 0 and modulus-1 such that if
  compare_fn(key1,key2) == 0 then hash_fn(key1) == hash_fn(key2).<p>
  There are five predefined hash and comparison functions in st.
  For keys as numbers:
  <pre>
	 st_numhash(key, modulus) { return (unsigned int) key % modulus; }
  </pre>
  <pre>
	 st_numcmp(x,y) { return (int) x - (int) y; }
  </pre>
  For keys as pointers:
  <pre>
	 st_ptrhash(key, modulus) { return ((unsigned int) key/4) % modulus }
  </pre>
  <pre>
	 st_ptrcmp(x,y) { return (int) x - (int) y; }
  </pre>
  For keys as strings:
  <pre>
         st_strhash(x,y) - a reasonable hashing function for strings
  </pre>
  <pre>
	 strcmp(x,y) - the standard library function
  </pre>
  It is recommended to use these particular functions if they fit your 
  needs, since st will recognize certain of them and run more quickly
  because of it.]

  SideEffects [None]

  SeeAlso     [st_init_table_with_params st_free_table]

******************************************************************************/
st_table *
st_init_table(ST_PFICPCP compare, ST_PFICPI hash)
{
    return st_init_table_with_params(compare, hash, ST_DEFAULT_INIT_TABLE_SIZE,
				     ST_DEFAULT_MAX_DENSITY,
				     ST_DEFAULT_GROW_FACTOR,
				     ST_DEFAULT_REORDER_FLAG);

} /* st_init_table */


/**Function********************************************************************

  Synopsis    [Create a table with given parameters.]

  Description [The full blown table initializer.  compare and hash are
  the same as in st_init_table. density is the largest the average
  number of entries per hash bin there should be before the table is
  grown.  grow_factor is the factor the table is grown by when it
  becomes too full. size is the initial number of bins to be allocated
  for the hash table.  If reorder_flag is non-zero, then every time an
  entry is found, it is moved to the top of the chain.<p>
  st_init_table(compare, hash) is equivelent to
  <pre>
  st_init_table_with_params(compare, hash, ST_DEFAULT_INIT_TABLE_SIZE,
			    ST_DEFAULT_MAX_DENSITY,
			    ST_DEFAULT_GROW_FACTOR,
			    ST_DEFAULT_REORDER_FLAG);
  </pre>
  ]

  SideEffects [None]

  SeeAlso     [st_init_table st_free_table]

******************************************************************************/
st_table *
st_init_table_with_params(
  ST_PFICPCP compare,
  ST_PFICPI hash,
  int size,
  int density,
  double grow_factor,
  int reorder_flag)
{
    int i;
    st_table *newt;

    newt = ALLOC(st_table, 1);
    if (newt == NIL(st_table)) {
	return NIL(st_table);
    }
    newt->compare = compare;
    newt->hash = hash;
    newt->num_entries = 0;
    newt->max_density = density;
    newt->grow_factor = grow_factor;
    newt->reorder_flag = reorder_flag;
    if (size <= 0) {
	size = 1;
    }
    newt->num_bins = size;
    newt->bins = ALLOC(st_table_entry *, size);
    if (newt->bins == NIL(st_table_entry *)) {
	FREE(newt);
	return NIL(st_table);
    }
    for(i = 0; i < size; i++) {
	newt->bins[i] = 0;
    }
    return newt;

} /* st_init_table_with_params */


/**Function********************************************************************

  Synopsis    [Free a table.]

  Description [Any internal storage associated with table is freed.
  It is the user's responsibility to free any storage associated
  with the pointers he placed in the table (by perhaps using
  st_foreach).]

  SideEffects [None]

  SeeAlso     [st_init_table st_init_table_with_params]

******************************************************************************/
void
st_free_table(st_table *table)
{
    st_table_entry *ptr, *next;
    int i;

    for(i = 0; i < table->num_bins ; i++) {
	ptr = table->bins[i];
	while (ptr != NIL(st_table_entry)) {
	    next = ptr->next;
	    FREE(ptr);
	    ptr = next;
	}
    }
    FREE(table->bins);
    FREE(table);

} /* st_free_table */


/**Function********************************************************************

  Synopsis    [Lookup up `key' in `table'.]

  Description [Lookup up `key' in `table'. If an entry is found, 1 is
  returned and if `value' is not nil, the variable it points to is set
  to the associated value.  If an entry is not found, 0 is returned
  and the variable pointed by value is unchanged.]

  SideEffects [None]

  SeeAlso     [st_lookup_int]

******************************************************************************/
int
st_lookup(st_table *table, void *key, void *value)
{
    int hash_val;
    st_table_entry *ptr, **last;

    hash_val = do_hash(key, table);

    FIND_ENTRY(table, hash_val, key, ptr, last);

    if (ptr == NIL(st_table_entry)) {
	return 0;
    } else {
	if (value != NIL(void)) {
	    *(char **)value = ptr->record;
	}
	return 1;
    }

} /* st_lookup */


/**Function********************************************************************

  Synopsis    [Lookup up `key' in `table'.]

  Description [Lookup up `key' in `table'.  If an entry is found, 1 is
  returned and if `value' is not nil, the variable it points to is
  set to the associated integer value.  If an entry is not found, 0 is
  return and the variable pointed by `value' is unchanged.]

  SideEffects [None]

  SeeAlso     [st_lookup]

******************************************************************************/
int
st_lookup_int(st_table *table, void *key, int *value)
{
    int hash_val;
    st_table_entry *ptr, **last;

    hash_val = do_hash(key, table);

    FIND_ENTRY(table, hash_val, key, ptr, last);
    
    if (ptr == NIL(st_table_entry)) {
	return 0;
    } else {
	if (value != NIL(int)) {
	    *value = (int) (long) ptr->record;
	}
	return 1;
    }

} /* st_lookup_int */


/**Function********************************************************************

  Synopsis    [Insert value in table under the key 'key'.]

  Description [Insert value in table under the key 'key'.  Returns 1
  if there was an entry already under the key; 0 if there was no entry
  under the key and insertion was successful; ST_OUT_OF_MEM otherwise.
  In either of the first two cases the new value is added.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
st_insert(st_table *table, void *key, void *value)
{
    int hash_val;
    st_table_entry *newt;
    st_table_entry *ptr, **last;

    hash_val = do_hash(key, table);

    FIND_ENTRY(table, hash_val, key, ptr, last);

    if (ptr == NIL(st_table_entry)) {
	if (table->num_entries/table->num_bins >= table->max_density) {
	    if (rehash(table) == ST_OUT_OF_MEM) {
		return ST_OUT_OF_MEM;
	    }
	    hash_val = do_hash(key, table);
	}
	newt = ALLOC(st_table_entry, 1);
	if (newt == NIL(st_table_entry)) {
	    return ST_OUT_OF_MEM;
	}
	newt->key = (char *)key;
	newt->record = (char *)value;
	newt->next = table->bins[hash_val];
	table->bins[hash_val] = newt;
	table->num_entries++;
	return 0;
    } else {
	ptr->record = (char *)value;
	return 1;
    }

} /* st_insert */


/**Function********************************************************************

  Synopsis    [Place 'value' in 'table' under the key 'key'.]

  Description [Place 'value' in 'table' under the key 'key'.  This is
  done without checking if 'key' is in 'table' already.  This should
  only be used if you are sure there is not already an entry for
  'key', since it is undefined which entry you would later get from
  st_lookup or st_find_or_add. Returns 1 if successful; ST_OUT_OF_MEM
  otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
st_add_direct(st_table *table, void *key, void *value)
{
    int hash_val;
    st_table_entry *newt;
    
    hash_val = do_hash(key, table);
    if (table->num_entries / table->num_bins >= table->max_density) {
	if (rehash(table) == ST_OUT_OF_MEM) {
	    return ST_OUT_OF_MEM;
	}
    }
    hash_val = do_hash(key, table);
    newt = ALLOC(st_table_entry, 1);
    if (newt == NIL(st_table_entry)) {
	return ST_OUT_OF_MEM;
    }
    newt->key = (char *)key;
    newt->record = (char *)value;
    newt->next = table->bins[hash_val];
    table->bins[hash_val] = newt;
    table->num_entries++;
    return 1;

} /* st_add_direct */

/**Function********************************************************************

  Synopsis    [Delete the entry with the key pointed to by `keyp'.]

  Description [Delete the entry with the key pointed to by `keyp'.  If
  the entry is found, 1 is returned, the variable pointed by `keyp' is
  set to the actual key and the variable pointed by `value' is set to
  the corresponding entry.  (This allows the freeing of the associated
  storage.)  If the entry is not found, then 0 is returned and nothing
  is changed.]

  SideEffects [None]

  SeeAlso     [st_delete_int]

******************************************************************************/
int
st_delete(st_table *table, void *keyp, void *value)
{
    int hash_val;
    char *key = *(char **)keyp;
    st_table_entry *ptr, **last;

    hash_val = do_hash(key, table);

    FIND_ENTRY(table, hash_val, key, ptr ,last);
    
    if (ptr == NIL(st_table_entry)) {
	return 0;
    }

    *last = ptr->next;
    if (value != NIL(void)) *(char **)value = ptr->record;
    *(char **)keyp = ptr->key;
    FREE(ptr);
    table->num_entries--;
    return 1;

} /* st_delete */


/**Function********************************************************************

  Synopsis    [Iterates over the elements of a table.]

  Description [For each (key, value) record in `table', st_foreach
  call func with the arguments
  <pre>
	  (*func)(key, value, arg)
  </pre>
  If func returns ST_CONTINUE, st_foreach continues processing
  entries.  If func returns ST_STOP, st_foreach stops processing and
  returns immediately. If func returns ST_DELETE, then the entry is
  deleted from the symbol table and st_foreach continues.  In the case
  of ST_DELETE, it is func's responsibility to free the key and value,
  if necessary.<p>

  The routine returns 1 if all items in the table were generated and 0
  if the generation sequence was aborted using ST_STOP.  The order in
  which the records are visited will be seemingly random.]

  SideEffects [None]

  SeeAlso     [st_foreach_item st_foreach_item_int]

******************************************************************************/
int
st_foreach(st_table *table, ST_PFSR func, char *arg)
{
    st_table_entry *ptr, **last;
    enum st_retval retval;
    int i;

    for(i = 0; i < table->num_bins; i++) {
	last = &table->bins[i]; ptr = *last;
	while (ptr != NIL(st_table_entry)) {
	    retval = (*func)(ptr->key, ptr->record, arg);
	    switch (retval) {
	    case ST_CONTINUE:
		last = &ptr->next; ptr = *last;
		break;
	    case ST_STOP:
		return 0;
	    case ST_DELETE:
		*last = ptr->next;
		table->num_entries--;	/* cstevens@ic */
		FREE(ptr);
		ptr = *last;
	    }
	}
    }
    return 1;

} /* st_foreach */


/**Function********************************************************************

  Synopsis    [Number hash function.]

  Description [Integer number hash function.]

  SideEffects [None]

  SeeAlso     [st_init_table st_numcmp]

******************************************************************************/
int
st_numhash(char *x, int size)
{
    return ST_NUMHASH(x, size);

} /* st_numhash */


/**Function********************************************************************

  Synopsis    [Pointer hash function.]

  Description [Pointer hash function.]

  SideEffects [None]

  SeeAlso     [st_init_table st_ptrcmp]

******************************************************************************/
int
st_ptrhash(char *x, int size)
{
    return ST_PTRHASH(x, size);

} /* st_ptrhash */


/**Function********************************************************************

  Synopsis    [Number comparison function.]

  Description [integer number comparison function.]

  SideEffects [None]

  SeeAlso     [st_init_table st_numhash]

******************************************************************************/
int
st_numcmp(const char *x, const char *y)
{
    return ST_NUMCMP(x, y);

} /* st_numcmp */


/**Function********************************************************************

  Synopsis    [Pointer comparison function.]

  Description [Pointer comparison function.]

  SideEffects [None]

  SeeAlso     [st_init_table st_ptrhash]

******************************************************************************/
int
st_ptrcmp(const char *x, const char *y)
{
    return ST_NUMCMP(x, y);

} /* st_ptrcmp */


/**Function********************************************************************

  Synopsis    [Initializes a generator.]

  Description [Returns a generator handle which when used with
  st_gen() will progressively return each (key, value) record in
  `table'.]

  SideEffects [None]

  SeeAlso     [st_free_gen]

******************************************************************************/
st_generator *
st_init_gen(st_table *table)
{
    st_generator *gen;

    gen = ALLOC(st_generator, 1);
    if (gen == NIL(st_generator)) {
	return NIL(st_generator);
    }
    gen->table = table;
    gen->entry = NIL(st_table_entry);
    gen->index = 0;
    return gen;

} /* st_init_gen */


/**Function********************************************************************

  Synopsis [returns the next (key, value) pair in the generation
  sequence. ]

  Description [Given a generator returned by st_init_gen(), this
  routine returns the next (key, value) pair in the generation
  sequence.  The pointer `value_p' can be zero which means no value
  will be returned.  When there are no more items in the generation
  sequence, the routine returns 0.

  While using a generation sequence, deleting any (key, value) pair
  other than the one just generated may cause a fatal error when
  st_gen() is called later in the sequence and is therefore not
  recommended.]

  SideEffects [None]

  SeeAlso     [st_gen_int]

******************************************************************************/
int
st_gen(st_generator *gen, void *key_p, void *value_p)
{
    int i;

    if (gen->entry == NIL(st_table_entry)) {
	/* try to find next entry */
	for(i = gen->index; i < gen->table->num_bins; i++) {
	    if (gen->table->bins[i] != NIL(st_table_entry)) {
		gen->index = i+1;
		gen->entry = gen->table->bins[i];
		break;
	    }
	}
	if (gen->entry == NIL(st_table_entry)) {
	    return 0;		/* that's all folks ! */
	}
    }
    *(char **)key_p = gen->entry->key;
    if (value_p != NIL(void)) {
	*(char **)value_p = gen->entry->record;
    }
    gen->entry = gen->entry->next;
    return 1;

} /* st_gen */


/**Function********************************************************************

  Synopsis    [Returns the next (key, value) pair in the generation
  sequence.]

  Description [Given a generator returned by st_init_gen(), this
  routine returns the next (key, value) pair in the generation
  sequence.  `value_p' must be a pointer to an integer.  The pointer
  `value_p' can be zero which means no value will be returned.  When
  there are no more items in the generation sequence, the routine
  returns 0.]

  SideEffects [None]

  SeeAlso     [st_gen]

******************************************************************************/
int 
st_gen_int(st_generator *gen, void *key_p, int *value_p)
{
    int i;

    if (gen->entry == NIL(st_table_entry)) {
	/* try to find next entry */
	for(i = gen->index; i < gen->table->num_bins; i++) {
	    if (gen->table->bins[i] != NIL(st_table_entry)) {
		gen->index = i+1;
		gen->entry = gen->table->bins[i];
		break;
	    }
	}
	if (gen->entry == NIL(st_table_entry)) {
	    return 0;		/* that's all folks ! */
	}
    }
    *(char **)key_p = gen->entry->key;
    if (value_p != NIL(int)) {
   	*value_p = (int) (long) gen->entry->record;
    }
    gen->entry = gen->entry->next;
    return 1;

} /* st_gen_int */


/**Function********************************************************************

  Synopsis    [Reclaims the resources associated with `gen'.]

  Description [After generating all items in a generation sequence,
  this routine must be called to reclaim the resources associated with
  `gen'.]

  SideEffects [None]

  SeeAlso     [st_init_gen]

******************************************************************************/
void
st_free_gen(st_generator *gen)
{
    FREE(gen);

} /* st_free_gen */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Rehashes a symbol table.]

  Description [Rehashes a symbol table.]

  SideEffects [None]

  SeeAlso     [st_insert]

******************************************************************************/
static int
rehash(st_table *table)
{
    st_table_entry *ptr, *next, **old_bins;
    int             i, old_num_bins, hash_val, old_num_entries;

    /* save old values */
    old_bins = table->bins;
    old_num_bins = table->num_bins;
    old_num_entries = table->num_entries;

    /* rehash */
    table->num_bins = (int) (table->grow_factor * old_num_bins);
    if (table->num_bins % 2 == 0) {
	table->num_bins += 1;
    }
    table->num_entries = 0;
    table->bins = ALLOC(st_table_entry *, table->num_bins);
    if (table->bins == NIL(st_table_entry *)) {
	table->bins = old_bins;
	table->num_bins = old_num_bins;
	table->num_entries = old_num_entries;
	return ST_OUT_OF_MEM;
    }
    /* initialize */
    for (i = 0; i < table->num_bins; i++) {
	table->bins[i] = 0;
    }

    /* copy data over */
    for (i = 0; i < old_num_bins; i++) {
	ptr = old_bins[i];
	while (ptr != NIL(st_table_entry)) {
	    next = ptr->next;
	    hash_val = do_hash(ptr->key, table);
	    ptr->next = table->bins[hash_val];
	    table->bins[hash_val] = ptr;
	    table->num_entries++;
	    ptr = next;
	}
    }
    FREE(old_bins);

    return 1;

} /* rehash */
