#include <ph.h>
#include <scene.h>

namespace ph {
namespace level {

static const float kLevelOffsetY = 1.0f;
static scene::Cube avatar_cube;
static Slice<scene::Rect> m_rects;
static int64  m_avatar_prim_index = -1;

void init() {
    m_rects = MakeSlice<scene::Rect>(32);
}

Slice<scene::Cube> load(const char* path) {
    auto cube_list = MakeSlice<scene::Cube>(16);

    auto* str = io::slurp(path);
    printf("level str\n%s\n", str);

    char errorbuf[1024];
    yajl_val root = yajl_tree_parse(str, errorbuf, sizeof(errorbuf));
    yajl_val val = root;
    if (!root) {
        printf("%s", errorbuf);
    }
    ph_assert(root != NULL);
    ph_assert(root->type == yajl_t_object);

    const char * paths[] = {"platforms", (const char*)0};

    auto array = yajl_tree_get(val, paths, yajl_t_array);

    ph_assert(array != NULL);

    size_t num_platforms = array->u.array.len;

    for (size_t i = 0; i < num_platforms; ++i) {
        auto* plat_array = array->u.array.values[i];
        ph_assert(plat_array->u.array.len == 3);

        float position[3];
        float size[3];

        position[2] = -8;
        size[1] = 0.125;
        size[2] = 1;

        for (int j = 0; j < 3; j++) {
            long long i = YAJL_GET_INTEGER(plat_array->u.array.values[j]);
            switch (j) {
            case 0:
                position[0] = (float) i;
                break;
            case 1:
                position[1] = (float) i;
                break;
            case 2:
                size[0] = (float) i / 2;
                break;
            }
        }
        scene::Cube cube;
        cube.center.x = position[0];
        cube.center.y = position[1] - kLevelOffsetY;
        cube.center.z = position[2];

        cube.sizes.x = size[0];
        cube.sizes.y = size[1];
        cube.sizes.z = size[2];

        append(&cube_list, cube);
    }

    // ========================================
    // Avatar start point
    // ========================================
    {
        double x, y, max;

        const char* x_paths[] = { "start", "x", NULL };
        auto obj = yajl_tree_get(val, x_paths, yajl_t_number);
        x = YAJL_GET_DOUBLE(obj);

        const char* below_paths[] = { "start", "below", NULL };
        obj = yajl_tree_get(val, below_paths, yajl_t_number);
        max = YAJL_GET_DOUBLE(obj);

        y = 1000;
        for (int64 i = 0; i < count(cube_list); ++i) {
            float maybe_y = cube_list[i].center.y + 2 * cube_list[i].sizes.y + 0.01f;
            if (maybe_y <= max && cube_list[i].center.x <= x) {
                y = maybe_y;
            }
        }

        avatar_cube.center.x = (float)x;
        avatar_cube.center.y = (float)y;
        avatar_cube.center.z = -7.5;

        avatar_cube.sizes.x = 0.125;
        avatar_cube.sizes.y = 0.125;
        avatar_cube.sizes.z = 0.125;
    }
    // ==================
    // Keep cube list as current m_rects
    // ==================
    if (count(m_rects) > 0) {
        clear(&m_rects);
    }
    for(int64 i = 0; i < count(cube_list); ++i) {
        append(&m_rects, cube_to_rect(cube_list[i]));
    }
    return cube_list;
}

void step(scene::Rect rect) {
    avatar_cube.center.x = rect.x + rect.w / 2;
    avatar_cube.center.y = rect.y + rect.h / 2;
    submit_primitive(&avatar_cube, scene::SubmitFlags_Update, m_avatar_prim_index);
}

}  // ns level

namespace gameplay {

using namespace glm;

enum Control {
    Control_none,
    Control_left,
    Control_right,
};

enum AvatarState {
    AvatarState_Idle,
    AvatarState_Jumping,
    AvatarState_Dead,
};

struct Avatar {
    scene::Rect rect;
    AvatarState state;
    vec3 velocity;
};

static Avatar m_avatar;
static float  m_avatar_step = 0.1f;
static int    m_pressed = Control_none;

void init(scene::Cube* avatar_cube) {
    m_avatar.rect.x = avatar_cube->center.x - avatar_cube->sizes.x;
    m_avatar.rect.y = avatar_cube->center.y - avatar_cube->sizes.y;
    m_avatar.rect.w = 2 * avatar_cube->sizes.x;
    m_avatar.rect.h = 2 * avatar_cube->sizes.y;
    level::m_avatar_prim_index = scene::submit_primitive(avatar_cube);
}

static void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int/*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
        m_pressed |= Control_left;
    }
    if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
        m_pressed |= Control_right;
    }
    if (key == GLFW_KEY_RIGHT && action == GLFW_RELEASE) {
        m_pressed &= ~Control_right;
    }
    if (key == GLFW_KEY_LEFT && action == GLFW_RELEASE) {
        m_pressed &= ~Control_left;
    }
}

bool is_on_top(Avatar avatar) {
    for (int64 i = 0; i < count(level::m_rects); ++i) {
        auto platform = level::m_rects[i];
        auto rect = avatar.rect;
        rect.y -= 0.03f;  // Make it collide
        if (collision_p(platform, rect)) {
            return true;
        }
    }
    return false;
}

void step() {
    level::step(m_avatar.rect);

    switch (m_pressed) {
    case Control_left:
        m_avatar.rect.x -= m_avatar_step;
        break;
    case Control_right:
        m_avatar.rect.x += m_avatar_step;
        break;
    default:
        break;
    }
    if (is_on_top(m_avatar)) {
        puts("Not falling! :)");
    } else {
        puts("Falling :(");
    }
}

}  // ns gameplay

}  // ns ph

using namespace ph;

static int g_resolution[2] = { 1920, 1080 };
static GLuint g_program = 0;

void chicken_idle() {
    scene::update_structure();
    scene::upload_everything();

    gameplay::step();

    vr::draw(g_resolution);



    //window::swap_buffers();
    //glFinish();
}

int main() {
    ph::init();
    window::init("Chicken", g_resolution[0], g_resolution[1],
        window::InitFlag(window::InitFlag_NoDecoration | window::InitFlag_OverrideKeyCallback
            //| window::InitFlag_IgnoreRift
            ));
    io::set_wasd_step(0.03f);

    glfwSetKeyCallback(ph::window::m_window, gameplay::key_callback);

    static const char* tracing = "pham/tracing.glsl";
    g_program = cs::init(g_resolution[0], g_resolution[1], &tracing, 1);

    vr::init(g_program);

    ovrHmd_AttachToWindow(vr::m_hmd, window::m_window, NULL, NULL);

    scene::init();
    level::init();

    auto cube_list = level::load("chicken/test.json");
    for(int64 i = 0; i < count(cube_list); ++i) {
        scene::Cube cube = cube_list[i];
        scene::submit_primitive(&cube);
    }

    gameplay::init(&level::avatar_cube);

    scene::update_structure();
    scene::upload_everything();

    window::main_loop(chicken_idle);

    window::deinit();
    vr::deinit();
    return 0;
}
