#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int child_process(int* pipe_line);

int main(){

    //printf("%d is created\n", getpid());
    int n = 2;
    printf("prime :%d\n", n); // print current prime

    int flag = 0;
    int pipe_line[2];

    for (int i = 3; i <= 35; ++i){
        if (  (i % n != 0) && (flag == 0) ){
            flag = 1;
            pipe(pipe_line);
            if ( fork() == 0){
                child_process(pipe_line);
            }else{
                close(pipe_line[0]); // no need to read from right neighboor
                write(pipe_line[1], &i, 4);
            }
        }else if ( (i % n != 0) && (flag == 1))
        {
            write(pipe_line[1], &i, 4);
        }
    }

    close(pipe_line[1]); // close the write end

    wait();
    close(1);
    exit();

    return 1;
}


int child_process(int* pipe_line){

    //printf("%d is created\n", getpid());
    int n = 0, m = 0;
    close(pipe_line[1]); // no need to write to left neighboor

    read(pipe_line[0], &n, 4);
    printf("prime %d\n", n); // print current prime

    int flag = 0;
    int new_pipe_line[2]; // create a new pipeline
    while (read(pipe_line[0], &m, 4)){ // when there is still some numbers fed from left neighboor
        if ( (m % n != 0) && ( flag == 0) ){ // found a new prime
            flag = 1;
            pipe(new_pipe_line);
            if ( fork() == 0 ){ // child process
                child_process(new_pipe_line);
            }else{
                close(new_pipe_line[0]); // no need to read from right neighboor
                write(new_pipe_line[1], &m, 4);
            }
        }else if ( (m % n != 0) && (flag == 1))
        {
            write(new_pipe_line[1], &m, 4);
        }
    }
    //printf("%d is going to be terminated\n", getpid());

    close(new_pipe_line[1]); // close the write end
    wait();
    close(1);
    exit();

    return 0;
}
