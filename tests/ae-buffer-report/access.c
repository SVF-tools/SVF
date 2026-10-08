extern void *malloc(unsigned long);
extern void *memmove(void *, const void *, unsigned long);
extern void *__memset_chk(void *, int, unsigned long, unsigned long);
extern char *strcpy(char *, const char *);
extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, unsigned long);

int main(int argc, char **argv)
{
#if defined(HEAP)
    char *buffer = malloc(8);
#elif defined(DYNAMIC)
    int length = 8;
    char buffer[length];
#else
    char buffer[8];
#endif
#if defined(COPY) || defined(COPY_SAFE) || defined(COPY_OFFSET)
    char destination[4];
#if defined(COPY_SAFE)
    memmove(destination, buffer, 4);
#elif defined(COPY_OFFSET)
    memmove(destination + 2, buffer, 4);
#else
    memmove(destination, buffer, 8);
#endif
    return destination[0];
#elif defined(SET)
    __memset_chk(buffer, 0, 9, 8);
#elif defined(STRING_COPY)
    char destination[4];
    strcpy(destination, "abcdefgh");
#elif defined(STRING_CAT)
    char destination[4];
    destination[0] = 'a';
    destination[1] = 'b';
    destination[2] = 0;
    strcat(destination, "cdef");
#elif defined(STRING_NCAT)
    char destination[4];
    destination[0] = 'a';
    destination[1] = 'b';
    destination[2] = 0;
    strncat(destination, "cdef", 4);
#elif defined(SAFE)
    char *volatile pointer = buffer + 7;
#elif defined(RANGE)
    char *volatile pointer = buffer + (argc > 1 ? 8 : 9);
#else
    char *volatile pointer = buffer + 9;
#endif
    return 0;
}
