/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
//TC06: Free data structure

#include "aliascheck.h"

typedef struct _list {
	    struct _list *next;
} List, *list;

list make_list(int n) {
	  int i;
	  list hd = (list)SAFEMALLOC(sizeof(List));
	  list p = hd; 
	  /*
		    for (i=0; i<n; i++) {
				    p->next = (list)malloc(sizeof(List)); 
				    p = p->next;
	   	  }
			  p->next = 0;
			  */
			    return hd;
}

void free_list(list hd) {
	    list p;
		    while (hd != 0) {
		      p = hd->next;
              free(hd);
              hd = p;
            }
}

void main() {
    list l = make_list(10);
    free_list(l);
}

