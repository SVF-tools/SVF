/**CHeaderFile*****************************************************************

  FileName    [st.h]

  PackageName [st]

  Synopsis    [Symbol table package.]

  Description [The st library provides functions to create, maintain,
  and query symbol tables.]

  SeeAlso     []

  Author      []

  Copyright   []

  Revision    [$Id: st.h,v 1.10 2004/01/02 07:40:31 fabio Exp fabio $]

******************************************************************************/

#ifndef ST_INCLUDED
#define ST_INCLUDED

/*---------------------------------------------------------------------------*/
/* Nested includes                                                           */
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define ST_DEFAULT_MAX_DENSITY 5
#define ST_DEFAULT_INIT_TABLE_SIZE 11
#define ST_DEFAULT_GROW_FACTOR 2.0
#define ST_DEFAULT_REORDER_FLAG 0
#define ST_OUT_OF_MEM -10000

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

typedef struct st_table_entry st_table_entry;
struct st_table_entry {
    char *key;
    char *record;
    st_table_entry *next;
};

typedef struct st_table st_table;
struct st_table {
    int (*compare)(const char *, const char *);
    int (*hash)(char *, int);
    int num_bins;
    int num_entries;
    int max_density;
    int reorder_flag;
    double grow_factor;
    st_table_entry **bins;
};

typedef struct st_generator st_generator;
struct st_generator {
    st_table *table;
    st_table_entry *entry;
    int index;
};

enum st_retval {ST_CONTINUE, ST_STOP, ST_DELETE};

typedef enum st_retval (*ST_PFSR)(char *, char *, char *);

typedef int (*ST_PFICPCP)(const char *, const char *); /* type for comparison function */

typedef int (*ST_PFICPI)(char *, int);     /* type for hash function */

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**Macro***********************************************************************

  Synopsis    [Checks whethere `key' is in `table'.]

  Description [Returns 1 if there is an entry under `key' in `table', 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [st_lookup]

******************************************************************************/
#define st_is_member(table,key) st_lookup(table,key,(char **) 0)


/**Macro***********************************************************************

  Synopsis    [Returns the number of entries in the table `table'.]

  Description [Returns the number of entries in the table `table'.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
#define st_count(table) ((table)->num_entries)


/**Macro***********************************************************************

  Synopsis    [Iteration macro.]

  Description [An iteration macro which loops over all the entries in
  `table', setting `key' to point to the key and `value' to the
  associated value (if it is not nil). `gen' is a generator variable
  used internally. Sample usage:
  <pre>
     	char *key, *value;
  </pre>
  <pre>
	st_generator *gen;
  </pre>
  <pre>

	st_foreach_item(table, gen, &key, &value) {
  </pre>
  <pre>
	    process_item(value);
  </pre>
  <pre>
	}
  </pre>
  ]

  SideEffects [None]

  SeeAlso     [st_foreach_item_int st_foreach]

******************************************************************************/
#define st_foreach_item(table, gen, key, value) \
    for(gen=st_init_gen(table); st_gen(gen,key,value) || (st_free_gen(gen),0);)


/**Macro***********************************************************************

  Synopsis    [Iteration macro.]

  Description [An iteration macro which loops over all the entries in
  `table', setting `key' to point to the key and `value' to the
  associated value (if it is not nil). `value' is assumed to be a
  pointer to an integer.  `gen' is a generator variable used
  internally. Sample usage:
  <pre>
     	char *key;
  </pre>
  <pre>
	int value;
  </pre>
  <pre>
	st_generator *gen;
  </pre>
  <pre>

	st_foreach_item_int(table, gen, &key, &value) {
  </pre>
  <pre>
	    process_item(value);
  </pre>
  <pre>
	}
  </pre>
  ]

  SideEffects [None]

  SeeAlso     [st_foreach_item st_foreach]

******************************************************************************/
#define st_foreach_item_int(table, gen, key, value) \
    for(gen=st_init_gen(table); st_gen_int(gen,key,value) || (st_free_gen(gen),0);)

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern st_table *st_init_table_with_params (ST_PFICPCP, ST_PFICPI, int, int, double, int);
extern st_table *st_init_table (ST_PFICPCP, ST_PFICPI); 
extern void st_free_table (st_table *);
extern int st_lookup (st_table *, void *, void *);
extern int st_lookup_int (st_table *, void *, int *);
extern int st_insert (st_table *, void *, void *);
extern int st_add_direct (st_table *, void *, void *);
extern int st_delete (st_table *, void *, void *);
extern int st_foreach (st_table *, ST_PFSR, char *);
extern int st_numhash (char *, int);
extern int st_ptrhash (char *, int);
extern int st_numcmp (const char *, const char *);
extern int st_ptrcmp (const char *, const char *);
extern st_generator *st_init_gen (st_table *);
extern int st_gen (st_generator *, void *, void *);
extern int st_gen_int (st_generator *, void *, int *);
extern void st_free_gen (st_generator *);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* ST_INCLUDED */
