/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

struct Buf {

	char* name;

}Buf;

struct Buf* readBuf(){

	struct Buf *buf = SAFEMALLOC(10);
	buf->name = SAFEMALLOC(10);

	return buf;

}

void freeBuf(struct Buf* buf){
	free(buf->name);
	free(buf);

}
void SerialReadBuf(){

	int n;
	while(n){
	
		struct Buf*	buf = readBuf();
			printf("%d%d",n,buf);

		if(n){
		//	continue;
		}
		freeBuf(buf);
		//free(buf->name);
		//free(buf);
	};


}


int main(){
SerialReadBuf();

}
