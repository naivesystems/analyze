#include <cstddef>
#include <cstdint>

void f1(int32_t);
void f2(int32_t*);

int main(){
    f1(0);
    f2(NULL);
    return 0;
}
