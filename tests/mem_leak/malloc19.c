/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// simplified from 177.mesa teximage.c read_color_image method

#include "aliascheck.h"

struct Img{
	int id;
	int* data;

};

struct Img* read_color_image(){
	struct Img* image = SAFEMALLOC(10);

	image->data = SAFEMALLOC(10);
	return image;
}

void free_image(struct Img* image){

	free(image->data);

	free(image);
}

int main(){
	int a;
	struct Img* image;
	image = read_color_image();

	free_image(image);


}
