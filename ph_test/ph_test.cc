#include <ph.h>

using namespace ph;
int main() {
    printf("Bytes used at start: %zu\n", ph::bytes_allocated());
    // Test slices
    {
        Slice<int> s = InitSlice<int>(1);
        for (int i = 0; i < 100000; ++i) {
            append(&s, i);
        }
        release(&s);
    }
    // Test memory tracking
    {
        auto* array = phalloc(int, 10);
        for (int i = 0; i < 10; ++i) {
            array[i] = i;
        }
        printf("Hello world! %d\n", array[5]);
        printf("Bytes used before free: %zu\n", ph::bytes_allocated());
        phree(array);
        printf("Bytes used after free : %zu\n", ph::bytes_allocated());
        ph_assert(true);
    }
    printf("Bytes used at end: %zu\n", ph::bytes_allocated());
    ph::quit(EXIT_SUCCESS);
}
