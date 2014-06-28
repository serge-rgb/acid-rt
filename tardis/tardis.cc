// 2014 Sergio Gonzalez

#include <ph.h>

using namespace ph;


int main() {
    ph::init();
    int size[] = {1920, 1080};
    window::init("Project TARDIS", size[0], size[1], window::InitFlag_default);

    const char* paths[] = {
        "test/compute.glsl",
    };

    vr::init(size[0], size[1], paths, 1);

    window::draw_loop(vr::draw);

    window::deinit();
}

