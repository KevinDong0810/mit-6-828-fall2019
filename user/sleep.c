#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[]){
    if (argc <= 1){
        printf("error, no input");
        exit();
    }

    int duration = atoi(argv[1]);
    sleep(duration);

    exit();
}