/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// TC03: interprocedural pointer aliasing 

#include "aliascheck.h"

void pointer(char **p, char* s)
{
		*p = s;
}

int ResourceLeak_TC03 (int arg1)
{
		char str[10] ="STRING";
		char *p1;
		char *p2;
		p1 = (char *)SAFEMALLOC(sizeof(char)*10);
		if( p1 == NULL) {
				return 1;
		}
		strcat(p1,str);
		pointer(&p2,p1);

		printf(" %s \n", p1);
		printf(" %s \n", p2);

		free(p2);
		return 0;	
}

int main()
{
		ResourceLeak_TC03(1);
			return 0;
}

