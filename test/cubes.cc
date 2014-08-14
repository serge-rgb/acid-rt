// 2014 Sergio Gonzalez

#include <ph.h>
#include <scene.h>

using namespace ph;

////////////////////////////////////////
//          -- tardis --
//  Entry point for the game.
////////////////////////////////////////

static GLuint g_program;
static int g_size[] = {1920, 1080};  // DK2 res

// Note 2: Workgroup size should be a multiple of workgroup size.
static int g_warpsize[] = {16, 8};
static GLfloat g_viewport_size[2];

void init(GLuint prog) {
    // Eye-to-lens
    glUseProgram(prog);
    glUniform1f(3, vr::m_default_eye_z);

    g_viewport_size[0] = GLfloat (g_size[0]) / 2;
    g_viewport_size[1] = GLfloat (g_size[1]);
    GLfloat size_m[2] = {
        vr::m_screen_size_m[0] / 2,
        vr::m_screen_size_m[1],
    };
    glUniform2fv(5, 1, size_m);     // screen_size_m
    glUniform1f(8, true);           // Occlude?
}

void draw() {
    GLCHK ( glUseProgram(g_program) );

    static unsigned int frame_index = 1;

    /* ovrFrameTiming frame_timing = ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index); */
    ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index);

    ovrPosef pose = ovrHmd_GetEyePose(vr::m_hmd, ovrEye_Left);

    // TODO -- Controls should be handled elsewhere
    auto q = pose.Orientation;
    GLfloat quat[4] {
        q.x, q.y, q.z, q.w,
    };
    GLfloat camera_pos[3];
    io::get_wasd_camera(quat, camera_pos);
    glUniform4fv(7, 1, quat);  // Camera orientation
    GLCHK ( glUniform3fv(10, 1, camera_pos) );  // update camera_pos

    GLCHK ( glUseProgram(g_program) );
    // Dispatch left viewport
    {
        GLfloat lens_center[2] = {
            (vr::m_screen_size_m[0] / 2) - (vr::m_renderinfo->LensSeparationInMeters / 2),
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

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Draw screen
    cs::fill_screen();

    window::swap_buffers();
    glFlush();
    GLCHK ( glFinish() );
    ovrHmd_EndFrameTiming(vr::m_hmd);
}

using namespace ph;
int main() {
    ph::init();
    vr::init();

    window::init("Cube grid", g_size[0], g_size[1],
            window::InitFlag(window::InitFlag_NoDecoration |
                window::InitFlag_OverrideKeyCallback));

    io::set_wasd_step(0.03f);
    glfwSetKeyCallback(ph::window::m_window, ph::io::wasd_callback);

    const char* paths[] = {
        "pham/tracing.glsl",
    };

    g_program = cs::init(g_size[0], g_size[1], paths, 1);

    scene::init();

    // Create test grid of cubes
    scene::Cube thing;
    {
        int x = 8;
        int y = 8;
        int z = 8;
        for (int i = 0; i < z; ++i) {
            for (int j = 0; j < y; ++j) {
                for (int k = 0; k < x; ++k) {
                    thing = {{i * 1.1, j * 1.1, -2 - k * 1.1}, {0.5, 0.5, 0.5}, -1};
                    scene::submit_primitive(&thing);
                }
            }
        }
        printf("INFO: Submitted %d polygons.\n", (3 * 12) + x * y * z * 12);
    }

    scene::update_structure();
    scene::upload_everything();

    init(g_program);

    window::main_loop(draw);

    window::deinit();

    vr::deinit();
    return 0;
}
