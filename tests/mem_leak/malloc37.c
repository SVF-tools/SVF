/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

struct Buf {

	char* name;

}Buf;

struct Buf* readBuf(){

	struct Buf *buf = NFRMALLOC(10);
	buf->name = SAFEMALLOC(10);

	return buf;

}

int main(){

	struct Buf * buf = readBuf();

	printf("%s",buf->name);
	struct Buf* buf1 = readBuf();

	free(buf->name);
	printf("%s",buf1->name);

}
