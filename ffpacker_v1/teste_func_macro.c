#include <fcntl.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdarg.h>

#define exit_error(...){fprintf(stderr, __VA_ARGS__);exit(1);}

int main (){
    exit_error("OOOlha la o %s tu é %s", "Rodrigo", "Foda!");
    return 0;
}