#include "ph_io.h"

#include "ph_base.h"


namespace ph
{
namespace io
{

const char* slurp(const char* path) {
    FILE* fd = fopen(path, "r");
    ph_expect(fd);
    if (!fd) {
        fprintf(stderr, "ERROR: couldn't slurp %s\n", path);
        ph::quit(EXIT_FAILURE);
    }
    fseek(fd, 0, SEEK_END);
    size_t len = (size_t)ftell(fd);
    fseek(fd, 0, SEEK_SET);
    const char* contents = phanaged(char, len);
    fread((void*)contents, len, 1, fd);
    return contents;
}

long get_microseconds() {
#ifdef _WIN32
    LARGE_INTEGER ticks;
    LARGE_INTEGER ticks_per_sec;
    QueryPerformanceFrequency(&ticks_per_sec);
    QueryPerformanceCounter(&ticks);

    ticks.QuadPart *= 1000000;  // Avoid precision loss;
    return (long)(ticks.QuadPart / ticks_per_sec.QuadPart);
#else
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long ns = tp.tv_nsec;
    return ns * 1000;
#endif
}

}  // ns io
}  // ns ph

