#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
int main()
{
    char *name = 0;
    char *pass = 0;
    while(1)
    {
        if(name) printf("name adress: %x\nname: %s\n",name,name);
        if(pass) printf("pass address: %x\npass: %s\n",pass,pass);
        printf("1: Username\n");
        printf("2: Password\n");
        printf("3: Reset\n");
        printf("4: Login\n");
        printf("5: Exit\n");
        printf("Selection? ");
        int num = 0;
        scanf("%d", &num);
        switch(num)
        {
            case 1:
                name = malloc(20*sizeof(char));
                printf("Insert Username: ");
                scanf("%254s", name);
                if(strcmp(name,"root") == 0)
                {
                    printf("root not allowed.\n");
                    strcpy(name,"");
                }
                break;
            case 2:
                pass = malloc(20*sizeof(char));
                printf("Insert Password: ");
                scanf("%254s", pass);
                break;
            case 3:
                free(pass);
                free(name);
                break;
            case 4:
                if(strcmp(name,"root") == 0)
                {
                    printf("You just used after free!\n");
		    system("/bin/sh");
                    exit(0);
 
                }
                break;
            case 5:
                exit(0);
        }
    }
 
    }
