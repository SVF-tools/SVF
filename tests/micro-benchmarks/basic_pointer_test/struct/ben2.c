struct agg{

	int* f;
	int* g;

}agg;


void foo(struct agg* agg1,int *p){

	agg1->f = p;
	agg1->g = p;

}

int main(){
	int a1, b1,c1;
	int *a, *b,*c;
	struct agg agg1;

	a = &a1;
	foo(&agg1,a);
}
