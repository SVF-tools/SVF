/*
 * Safe malloc when considering exit as safe
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

// from spec2000 
void *my_malloc (int size) {

	 void *ret;

	  if ((ret = SAFEMALLOC (size)) ==0 ) {
		      printf("Error:  Unable to malloc memory.  Aborting.\n");
			      exit (1);
				   }
	   return (ret);
}

int main(){


	int * side_ordering = (int *) my_malloc (10 * sizeof(int));

	free(side_ordering);
}
