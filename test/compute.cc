// 2014 Sergio Gonzalez

#include <ph.h>

using namespace ph;

int main() {
    ph::init();
    int size[] = {512, 512};
    window::init("Test", size[0], size[1], window::InitFlag_default);

    const char* path = "test/compute.glsl";

    vr::init(size[0], size[1], &path, 1);

    window::draw_loop(vr::draw);

    window::deinit();
}

