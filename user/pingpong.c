#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(){
    int p[2]; // fd
    char buf[5];
    pipe(p); // create a pipe
    sleep(5);
    if ( fork() == 0 ) { // child process
        read(p[0], buf, 4); // read a byte from the pipe
        printf("%d: received %s\n", getpid(), buf);
        close(p[0]);
        write(p[1], "pong", 4); // write a byte to the pipe
        close(p[1]);
        exit(); // remember to call exit() to release the resource

    }else{ // parent process

        write(p[1], "ping", 4);
        close(p[1]);
        wait();
        read(p[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        close(p[0]);
        exit();
    }

    return 1;
}