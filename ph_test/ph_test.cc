// OVERRRIDES
//#define PH_SLICES_ARE_MANUAL
#include <ph.h>

using namespace ph;

////////////////////////////////////////
// Used to check that core constructs
// work as intended.
////////////////////////////////////////

int main() {
//    GC_enable_incremental();
    printf("Boehm GC version is %d.%d.%d\n", GC_VERSION_MAJOR, GC_VERSION_MINOR, GC_VERSION_MICRO);
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
        // Slices are by default GC'd
        // so there is no need for this, unless the test
        // uncomments the define on PH_SLICES_ARE_MANUAL
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
