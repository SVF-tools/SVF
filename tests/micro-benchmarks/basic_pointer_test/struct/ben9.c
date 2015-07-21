typedef struct link{
	int a;
	struct link* next;
}link;

void connect(link* first,link* second){
	first->next = second;
}

void dog(){
	link o1,o2;
	link* l1 = &o1;
	link* l2 = &o2;
	connect(l1,l2);
}

void cat(){

	link o3,o4;
	link* l3 = &o3;
	link* l4 = &o4;
	connect(l3,l4);
}


int main(){

	



}
