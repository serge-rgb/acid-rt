#include <ph.h>

using namespace ph;

////////////////////////////////////////
// Used to check that core constructs
// work as intended.
////////////////////////////////////////

int main() {
    ph::init();
    printf("Boehm GC version is %d.%d.%d\n", GC_VERSION_MAJOR, GC_VERSION_MINOR, GC_VERSION_MICRO);
    printf("Bytes used at start: %llu\n", memory::bytes_allocated());
    // Test slices
    {
        // Test append =======================================
        auto s = MakeSlice<int>(1);
        for (int i = 0; i < 100000; ++i) {
            append(&s, i);
        }
        ph_assert(count(s) == 100000);
        for (int i = 0; i < count(s); ++i) {
            ph_assert(s[i] == i);
        }

        // Test slice func. ==================================
        auto c = slice(s, 0, 10);
        ph_assert(count(c) == 10);
        for (int i = 0; i < count(c); ++i) {
            ph_assert(c[i] == i);
        }
        c = slice(s, 42, 1729);
        ph_assert(count(c) == (1729 - 42));
        for (int i = 0; i < count(c); ++i) {
            ph_assert(c[i] == i + 42);
        }
        // Slices are by default GC'd
        // so there is no need for this, unless the test
        // uncomments the define on PH_SLICES_ARE_MANUAL
#ifdef PH_SLICES_ARE_MANUAL
        release(&s);
#endif

    {
        char debug[] = "debug";;
        ph::logf("This is my %s function\n", debug);
    }

    }
    // Test memory tracking
    {
        auto* array = phalloc(int, 10);
        for (int i = 0; i < 10; ++i) {
            array[i] = i;
        }
        printf("Hello world! %d\n", array[5]);
        printf("Bytes used before free: %llu\n", memory::bytes_allocated());
        phree(array);
        printf("Bytes used after free : %llu\n", memory::bytes_allocated());
    }
    printf("Bytes used at end: %llu\n", memory::bytes_allocated());
    ph::quit(EXIT_SUCCESS);
}

