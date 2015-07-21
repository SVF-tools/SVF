/*
 * Safe malloc, global
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

//TC07: global escape 
#include "aliascheck.h"

int *G = NULL;

int* make_global(){
	  return &G;
}

int ResourceLeak_TC07 (int *p)
{
	  int **gp = make_global();
	    *gp = p;
}

int main()
{
	  int *p;
      p = (int *) SAFEMALLOC(10);
	  ResourceLeak_TC07 (p);
      return 0;
}
