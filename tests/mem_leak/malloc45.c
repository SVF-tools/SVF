/*
 * Safe malloc and never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

char** readBuf(){

	char ** mBuf = SAFEMALLOC(10);
	*mBuf = NFRMALLOC(10);
	return mBuf;

}

void freeBuf(char** fBuf){

//	free(*fBuf);
	free(fBuf);
}

void SerialReadBuf(){
	int n;
	for(n = 0; n < 100; n++){

		char** buf = readBuf();
		if(*buf!='\n'){
			printf("%s",*buf);
		}
		else{
		//	continue;
		}
		//free(buf);
		//free(*buf);
		freeBuf(buf);
	}
}

int main(){

	SerialReadBuf();
}



