int fib (int n){
    if (n<0){
        return -1;
    }
    if (n<=1){
        return 1;
    }
    return fib(n-1)+fib(n-2);
}
int main(){
    int a =1;
    int b =2;
    int c = fib (a+b);
    return c;
}