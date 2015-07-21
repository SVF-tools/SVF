void foo(int **s,int **c){
    s = c;
}
int main(){

    int* a;
    int* b;
    int**c;
    
    int x,y;

    int *w;
    int **s;
    int u;
    a=&x;
    b=&y;
    c=&a;
    s=&w;
    w=&u;
    u = 9;
    c=s;
    foo(s,c); 
    if(x==5){
    	**c = 10;
    }
    else{
	*b = 100;
    }

    while(u>10){
	*w = 100;
    }
    return 0;

}
