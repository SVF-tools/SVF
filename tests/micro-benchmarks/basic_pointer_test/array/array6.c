double **bus;
int main(){
	int i;
	bus = (double**) malloc(100);

	for(i = 0; i < 10; i++){
		bus[i] = (int*)malloc(10);
	}
	for(i = 0; i < 10; i++){
		free(bus[i]);
	}

	free(bus);


}
