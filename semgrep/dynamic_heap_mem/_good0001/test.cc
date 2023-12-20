#include <cstdio>
#define new 1
#define free(a) (printf(#a))

void malloc() {}

int main()
{
    int x = new;
    free(x);
    malloc();
    return 0;
}
