#include <malloc.h>

int main(int argc, char *argv[])
{
    unsigned int bytes = 1024 * 1024;
    char *buf;

    /* allocate memory with malloc instead of alloca */
    buf = malloc(bytes);

    return 0;
}
