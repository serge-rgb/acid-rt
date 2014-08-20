#include <ph.h>
#include <scene.h>

namespace ph {
namespace level {

static const float kLevelOffsetY = 1.0f;

Slice<scene::Cube> load(const char* path) {
    auto cube_list = MakeSlice<scene::Cube>(16);

    auto* str = io::slurp(path);
    printf("level str\n%s\n", str);

    char errorbuf[1024];
    yajl_val val = yajl_tree_parse(str, errorbuf, sizeof(errorbuf));
    if (!val) {
        printf("%s", errorbuf);
    }
    ph_assert(val != NULL);
    ph_assert(val->type == yajl_t_object);

    const char * paths[] = {"platforms", (const char*)0};

    auto array = yajl_tree_get(val, paths, yajl_t_array);

    ph_assert(array != NULL);

    size_t num_platforms = array->u.array.len;

    printf("nelems %Iu\n", num_platforms);

    for (size_t i = 0; i < num_platforms; ++i) {
        auto* plat_array = array->u.array.values[i];
        ph_assert(plat_array->u.array.len == 3);

        float position[3];
        float size[3];

        position[2] = -8;
        size[1] = 0.25;
        size[2] = 2;

        printf("[ ");
        for (int j = 0; j < 3; j++) {
            long long i = YAJL_GET_INTEGER(plat_array->u.array.values[j]);
            printf("%d ", i);
            switch (j) {
            case 0:
                position[0] = (float) i;
                break;
            case 1:
                position[1] = (float) i;
                break;
            case 2:
                size[0] = (float) i;
                break;
            }
        }
        printf("]\n");
        scene::Cube cube;
        cube.center.x = position[0];
        cube.center.y = position[1] - kLevelOffsetY;
        cube.center.z = position[2];

        cube.sizes.x = size[0];
        cube.sizes.y = size[1];
        cube.sizes.z = size[2];

        append(&cube_list, cube);
    }
    return cube_list;
}
}  // ns level
}  // ns ph

using namespace ph;

static int g_resolution[2] = { 1920, 1080 };
static GLuint g_program = 0;

void chicken_idle() {
    vr::draw(g_resolution);

    scene::update_structure();
    scene::upload_everything();
    //window::swap_buffers(); 
    //glFinish();
}

int main() {
    ph::init();
    window::init("Chicken", g_resolution[0], g_resolution[1],
        window::InitFlag(window::InitFlag_NoDecoration | window::InitFlag_OverrideKeyCallback
            | window::InitFlag_IgnoreRift
            ));
    io::set_wasd_step(0.03f);

    glfwSetKeyCallback(ph::window::m_window, ph::io::wasd_callback);

    static const char* tracing = "pham/tracing.glsl";
    g_program = cs::init(g_resolution[0], g_resolution[1], &tracing, 1);

    vr::init(g_program);

    ovrHmd_AttachToWindow(vr::m_hmd, window::m_window, NULL, NULL);

    scene::init();

    auto cube_list = level::load("chicken/test.json");
    for(int64 i = 0; i < count(cube_list); ++i) {
        scene::Cube cube = cube_list[i];
        scene::submit_primitive(&cube);
    }
    scene::update_structure();
    scene::upload_everything();

    window::main_loop(chicken_idle);

    window::deinit();
    vr::deinit();
    return 0;
}

