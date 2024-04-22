int add(int a, int b)
{
    return a + b;
}    
int fib(int n)
{
    if (n < 0)
    {
        return -1;
    }
    if (n <= 1)
    {
        return 1;
    }
    return add(1,2);
}

int main()
{
    int a = 1;
    int b = 2;
    int c = fib(a + b);
    int d=c;
    d= fib(5);
    int m=5;
    while (a<m){
        a=a+1;
    }
    return c;
}