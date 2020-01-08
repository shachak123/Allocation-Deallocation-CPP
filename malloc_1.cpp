#include <cstddef>
#include <unistd.h>

const int FAIL = -1;
const size_t MAX_NAIVE_SIZE = 100000000;

void* malloc(size_t size){
    if (size == 0 || size > MAX_NAIVE_SIZE)
        return NULL;
    void *PB = sbrk(size);
    return ((PB == (void*)FAIL) ? NULL : PB);
}
