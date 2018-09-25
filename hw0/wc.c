#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#define BUF_SIZE 64
int main(int argc, char *argv[]) {
    FILE *f;
    int lines,words,bytes,chars;
    char buf[BUF_SIZE];
    int flag,ch;
    struct stat file_infos;
    flag = lines = bytes = words = chars = 0;
    
    if(argc == 2) {
        int ret = stat(argv[1],&file_infos); 
        if (ret != 0) {
            printf("can't find file %s\n",argv[1]);
            return 0;
        }
        mode_t mode = file_infos.st_mode;
        if (S_ISDIR(mode)){
               printf("error : %s is a directory\n",argv[1]);
        } else {
            bytes = file_infos.st_size;
            if((f = fopen(argv[1],"r")) != NULL) {
                while((ch = fgetc(f)) != EOF){
                    chars++;
                    if(ch == ' ' || ch == '\t' ) {
                        if (flag == 1) {
				flag = 0;
				words++;
                        }
			continue;
                    }
                    if(ch == '\n') {
                        lines++;
			if(flag == 1) {
                        words++;
			}
			flag = 0;
			continue;
                    }
		    if(flag ==0) {
		       flag = 1;
		    }

                }
            }
        }
    } else if (argc == 1) {

    }
    printf("%d %d %d %s\n",lines,words,bytes,argv[1]);
    return 0;
}
