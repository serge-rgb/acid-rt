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

}  // ns io
}  // ns ph

