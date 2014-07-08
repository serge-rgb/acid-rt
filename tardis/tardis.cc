// 2014 Sergio Gonzalez

#include <ph.h>

using namespace ph;

namespace vr {

static const OVR::HMDInfo* m_hmdinfo;
static OVR::HmdRenderInfo* m_renderinfo;
static ovrHmd              m_hmd;
static float               m_default_eye_z;  // Getting the default FOV, calculate eye distance from plane.

void init() {

    ///////////////
    // Allocate crap
    // Not deallocated. This should only be called once or the caller is dumb and ugly.
    m_renderinfo = new OVR::HmdRenderInfo();
    ///////////////

    if (!ovr_Initialize()) {
        ph::phatal_error("Could not initialize OVR\n");
    }
    m_hmd = ovrHmd_Create(0);
    ovrHmdDesc desc;
    ovrHmd_GetDesc(m_hmd, &desc);

    ovrHmd_StartSensor(m_hmd, 0x1111/*all*/, ovrHmd_GetEnabledCaps(m_hmd));

    auto fovPort_l = desc.DefaultEyeFov[0];
    auto fovPort_r = desc.DefaultEyeFov[1];

    ovrEyeRenderDesc rdesc[2];

    rdesc[0] = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Left, fovPort_l);
    rdesc[1] = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Right, fovPort_r);

    hmdState* state = (hmdState*)m_hmd;
    m_hmdinfo = &state->HMDInfo;

    // Default fov (looking down)
    float hvfov = (fovPort_r.DownTan + fovPort_l.DownTan) / 2.0f;
    printf("Default half fov (looking down): %f\n", hvfov);
    float h = m_hmdinfo->ScreenSizeInMeters.h;
    printf("Physical height/2 %f\n", h/2);

    // Let's get the eye distance.
    m_default_eye_z = h / (1 * hvfov);
    printf("eye z should be roughly %f\n", m_default_eye_z);

    *m_renderinfo = GenerateHmdRenderInfoFromHmdInfo(*m_hmdinfo);
    // Pass frameIndex == 0 if ovrHmd_GetFrameTiming isn't being used. Otherwise,
// pass the same frame index as was used for GetFrameTiming on the main thread.

}

void deinit() {
    ovrHmd_StopSensor(m_hmd);
    ovrHmd_Destroy(m_hmd);
    ovr_Shutdown();
}

}  // ns vr

enum {
    Control_W = 1 << 0,
    Control_A = 1 << 1,
    Control_S = 1 << 2,
    Control_D = 1 << 3,
};

static int pressed = 0;

static void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    if (key == GLFW_KEY_W && action == GLFW_PRESS) {
       pressed |= Control_W;
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS) {
       pressed |= Control_A;
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
       pressed |= Control_S;
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
       pressed |= Control_D;
    }
    if (key == GLFW_KEY_W && action == GLFW_RELEASE) {
       pressed &= ~Control_W;
    }
    if (key == GLFW_KEY_A && action == GLFW_RELEASE) {
       pressed &= ~Control_A;
    }
    if (key == GLFW_KEY_S && action == GLFW_RELEASE) {
       pressed &= ~Control_S;
    }
    if (key == GLFW_KEY_D && action == GLFW_RELEASE) {
       pressed &= ~Control_D;
    }
}

static GLuint g_program;
static GLuint g_green_prog;  // Program to fill the viewport green (debug mem barrier)
/* static int g_size[] = {1920, 1080}; */
static int g_size[] = {1280, 800};
/* static int g_size[] = {640, 400}; */
// Note: perf is really sensitive about this. Runtime tweak?
static int g_warpsize[] = {8, 8};
static GLfloat g_viewport_size[2];

namespace scene {
struct vec3 {
    float x;
    float y;
    float z;
    float _padding;
};

struct Triangle {
    vec3 p0;
    vec3 p1;
    vec3 p2;
};

static Slice<Triangle> g_triangle_pool;

void init() {
    g_triangle_pool = MakeSlice<Triangle>(1);
    append(&g_triangle_pool, {
        {0,0,-2,0},
        {-0.1f,0,-2,0},
        {0,0.1f,-2,0},
    });
    append(&g_triangle_pool, {
        {0,0,-2,0},
        {0,-0.1f,-1.8f,0},
        {-0.1f,0,-2,0},
    });
}

} // ns scene

void init(GLuint prog) {
    // Eye-to-lens
    glUseProgram(prog);
    float eye_to_lens = vr::m_default_eye_z; // Calculated for default FOV
    printf("Eye to lens is: %f\n", eye_to_lens);
    glUniform1f(3, eye_to_lens);  // eye to lens.

    g_viewport_size[0] = GLfloat (g_size[0]) / 2;
    g_viewport_size[1] = GLfloat (g_size[1]);
    GLfloat size_m[2] = {
        vr::m_renderinfo->ScreenSizeInMeters.w / 2,
        vr::m_renderinfo->ScreenSizeInMeters.h,
    };
    glUniform2fv(5, 1, size_m);  // screen_size in meters
    glUniform1f(8, true);  // Occlude?

    GLuint point_buffer;
    glGenBuffers(1, &point_buffer);
    GLCHK();
    GLCHK ( glBindBuffer(GL_SHADER_STORAGE_BUFFER, point_buffer) );
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                GLsizeiptr(sizeof(scene::Triangle) * scene::g_triangle_pool.n_elems),
                (GLvoid*)scene::g_triangle_pool.ptr, GL_DYNAMIC_COPY);
    GLCHK ( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, point_buffer) );
}

void draw() {
    GLCHK ( glUseProgram(g_program) );

    static GLfloat sphere_y = 0.0f;
    static float step_var = 0.0;
    sphere_y = 0.2f * sinf(step_var);
    glUniform1f(4, sphere_y);
    step_var += 0.05;

    static unsigned int frame_index = 1;
    if (frame_index == 1) {
        ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index);
    }

    ovrFrameTiming frame_timing = ovrHmd_BeginFrame(vr::m_hmd, frame_index);
    frame_timing = ovrHmd_GetFrameTiming(vr::m_hmd, frame_index);

    auto sdata = ovrHmd_GetSensorState(vr::m_hmd, frame_timing.ScanoutMidpointSeconds);

    ovrPosef pose = sdata.Predicted.Pose;

    auto q = pose.Orientation;
    GLfloat quat[4] {
        q.x, q.y, q.z, q.w,
    };
    glUniform4fv(7, 1, quat);

    static GLfloat camera_pos[2] = {0, 0};
    static GLfloat cam_step = 0.03f;
    auto glm_q = glm::quat(quat[0], quat[1], quat[2], quat[3]);
    GLfloat angle = glm::eulerAngles(glm_q)[1];
    // Degrees to radian.
    angle = (-angle / 180) * 3.141526f;
    GLfloat cam_step_x;
    GLfloat cam_step_y;

    if (pressed & Control_W) {
        cam_step_x = cam_step * cosf(angle);
        cam_step_y = cam_step * sinf(angle);
        camera_pos[0] -= cam_step_x;
        camera_pos[1] -= cam_step_y;
    }
    if (pressed & Control_S) {
        cam_step_x = cam_step * cosf(angle);
        cam_step_y = cam_step * sinf(angle);
        camera_pos[0] += cam_step_x;
        camera_pos[1] += cam_step_y;
    }
    if (pressed & Control_A) {
        cam_step_x = cam_step * cosf(angle + 3.14f/2);
        cam_step_y = cam_step * sinf(angle + 3.14f/2);
        camera_pos[0] -= cam_step_x;
        camera_pos[1] -= cam_step_y;
    }
    if (pressed & Control_D) {
        cam_step_x = cam_step * cosf(angle + 3.14f/2);
        cam_step_y = cam_step * sinf(angle + 3.14f/2);
        camera_pos[0] += cam_step_x;
        camera_pos[1] += cam_step_y;
    }
    glUniform2fv(10, 1, camera_pos);  // update camera_pos

    // Draw green screen
    GLCHK ( glUseProgram(g_green_prog) );
    {
        GLfloat lens_center[2] = {
            (vr::m_renderinfo->ScreenSizeInMeters.w / 2) -
                (vr::m_renderinfo->LensSeparationInMeters / 2),
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, 0);  // x_offset
        GLCHK ( glDispatchCompute(GLuint(g_size[0] / g_warpsize[0]),
                    GLuint(g_size[1] / g_warpsize[1]), 1) );
    }
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    GLCHK ( glUseProgram(g_program) );
    // Dispatch left viewport
    {
        GLfloat lens_center[2] = {
            (vr::m_renderinfo->ScreenSizeInMeters.w / 2) -
                (vr::m_renderinfo->LensSeparationInMeters / 2),
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, 0);  // x_offset
        GLCHK ( glDispatchCompute(GLuint(g_viewport_size[0] / g_warpsize[0]),
                    GLuint(g_viewport_size[1] / g_warpsize[1]), 1) );
    }
    // Dispatch right viewport
    {
        GLfloat lens_center[2] = {
            vr::m_renderinfo->LensSeparationInMeters / 2,
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, ((GLfloat)g_viewport_size[0]));  // x_offset
        GLCHK ( glDispatchCompute(GLuint(g_viewport_size[0] / g_warpsize[0]),
                    GLuint(g_viewport_size[1] / g_warpsize[1]), 1) );
    }
    frame_index++;
    ovrHmd_EndFrame(vr::m_hmd);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    // Draw screen
    cs::fill_screen();
}

int main() {
    vr::init();
    ph::init();

    window::init("Project TARDIS", g_size[0], g_size[1],
            window::InitFlag(window::InitFlag_NoDecoration |
                window::InitFlag_OverrideKeyCallback));

    glfwSetKeyCallback(ph::window::m_window, key_callback);

    const char* paths[] = {
        "tardis/main.glsl",
    };

    g_program = cs::init(g_size[0], g_size[1], paths, 1);
    const char* test_path = "tardis/green.glsl";
    g_green_prog = cs::init(g_size[0], g_size[1], &test_path, 1);

    scene::init();

    init(g_program);
    init(g_green_prog);

    window::draw_loop(draw);

    window::deinit();

    vr::deinit();
}

