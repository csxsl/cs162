#include <stdio.h>
#include <sys/resource.h>
int main() {
    struct rlimit limi;
    getrlimit(RLIMIT_STACK,&limi);
    printf("stack size: %ld\n", limi.rlim_cur);
    getrlimit(RLIMIT_NPROC,&limi);
    printf("process limit: %ld\n", limi.rlim_cur);
    getrlimit(RLIMIT_NOFILE,&limi);
    printf("max file descriptors: %ld\n", limi.rlim_cur);
    return 0;
}
