struct agg{

	int* f;
	int *g;

}agg;

int main(){
	int a1, b1,c1;
	int *a, *b,*c;
	struct agg agg1[100];
	a = &a1;
	b = &b1;
	agg1[1].f = a;
	agg1[1].g = b;
	//agg1[0].f = &c;

}
