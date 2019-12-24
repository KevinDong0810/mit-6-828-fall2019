#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


// split the path and get the file name
char* fmtname(char *path){
  char *p;

  // Find first character after last slash.
  for( p = path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  return p;
}

/* path should be relative to the root directory

*/

void helper( char* path, char* file_pattern){

    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // open the path
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // get the stat
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // if st is a file

    switch(st.type){

        case T_FILE:
            // check whether pattern matches
            //printf("file: %s\n", path);
            if (strcmp(fmtname(path), file_pattern) == 0 ){
                printf("%s\n", path);
            }
            break;

        case T_DIR:
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';

            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0)
                    continue;
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
                //printf("dir item %s\n", de.name);
                memmove(p, de.name, sizeof(de.name));
                p[sizeof(de.name)-1] = 0;
                //printf("%s\n", buf);
                helper(buf, file_pattern);
            }
        break;
    }

    close(fd);
    return;
}

int main(int argc, char* argv[]){

    if (argc < 3){
        printf("no enough data\n");
        exit();
    }

    
    char* root_dir = argv[1];
    char* file_pattern = argv[2];

    helper(root_dir, file_pattern);

    exit();
}