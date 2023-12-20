#include <alloca.h>

int main(int argc, char *argv[])
{
    unsigned int bytes = 1024 * 1024;
    char *buf;

    /* allocate memory */
    buf = alloca(bytes);

    return 0;
}
