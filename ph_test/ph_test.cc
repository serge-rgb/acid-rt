#include <ph.h>

int main() {
    auto* array = phalloc(int, 10);
    for (int i = 0; i < 10; ++i) {
        array[i] = i;
    }
    printf("Hello world! %d\n", array[5]);
    printf("Bytes used before free: %zu\n", ph::bytes_allocated());
    phree(array);
    printf("Bytes used after free : %zu\n", ph::bytes_allocated());
    ph::quit(EXIT_SUCCESS);
}
