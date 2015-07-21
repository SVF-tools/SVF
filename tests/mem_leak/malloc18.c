/*
 * Never free
 * Author: Yule Sui
 * Date: 02/04/2014
 */

// simplified from 177.mesa teximage.c read_color_image method

#include "aliascheck.h"

struct Img{
	int id;
	int* data;

};

struct Img* read_color_image(int* p){
	*p = 100;
	struct Img* image = SAFEMALLOC(10);

	image->data = NFRMALLOC(10);
	return image;
}

void free_image(struct Img* image){

	//free(image->data);

	free(image);
}

int main(){
	int* x;
	int b,c;
	b = 10;
	c = 20;
	if(x)
	x = &b;
	else
		x= &c;
	struct Img* image = read_color_image(x);

	free_image(image);


}
