#include <ph.h>


using namespace ph;

////////////////////////////////////////
// === Use with valgrind.
// Used to check that core constructs
// work as intended.
////////////////////////////////////////

int main() {
    printf("Bytes used at start: %zu\n", memory::bytes_allocated());
    // Test slices
    {
        auto s = MakeSlice<int>(1);
        for (int i = 0; i < 100000; ++i) {
            append(&s, i);
        }
        ph_assert(count(&s) == 100000);
        for (int i = 0; i < count(&s); ++i) {
            ph_assert(s[i] == i);
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
        printf("Bytes used before free: %zu\n", memory::bytes_allocated());
        phree(array);
        printf("Bytes used after free : %zu\n", memory::bytes_allocated());
        ph_assert(true);
    }
    printf("Bytes used at end: %zu\n", memory::bytes_allocated());
    ph::quit(EXIT_SUCCESS);
}
