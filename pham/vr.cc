#include "vr.h"

// Note 2: Workgroup size should be a multiple of workgroup size.
static int g_warpsize[] = {16, 8};

namespace ph {
namespace vr {

static GLuint m_quad_vao;
static GLuint m_quad_program;
static GLuint m_compute_program;

static GLuint m_size[2];
static float                     m_default_eye_z;     // Eye distance from plane.
static const OVR::HMDInfo*       m_hmdinfo;
static const OVR::HmdRenderInfo* m_renderinfo;
static float                     m_screen_size_m[2];  // Screen size in meters
static GLuint                    m_program = 0;

ovrHmd m_hmd;

void init(int width, int height) {
    const char* paths[] = {
        "pham/tracing.glsl",
    };
    init_with_shaders(width, height, paths, 1);
}

void init_with_shaders(int width, int height, const char** shader_paths, int num_shaders) {
    enum Location {
        Location_pos = 0,
        Location_screen_size = 0,
        Location_tex = 1,
    };

    m_size[0] = (GLuint)width;
    m_size[1] = (GLuint)height;

    // Create / fill texture
    {
        // Create texture
        GLuint texobj;
        GLCHK (glActiveTexture (GL_TEXTURE0) );
        GLCHK (glGenTextures   (1, &texobj) );
        GLCHK (glBindTexture   (GL_TEXTURE_2D, texobj) );

        // Note for the future: These are needed.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Pass a null pointer, texture will be filled by compute shader
        // Note: Internal format was vital for compute shader.
        GLCHK ( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height,
                    0, GL_RGBA, GL_FLOAT, NULL) );

        // Bind it to image unit
        GLCHK ( glBindImageTexture(0, texobj, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F) );
    }
    // Create main program
    {
        enum {
            vert,
            frag,
        };
        GLuint shaders[2];
        const char* path[2];
        path[vert] = "pham/quad.v.glsl";
        path[frag] = "pham/quad.f.glsl";
        shaders[vert] = ph::gl::compile_shader(path[vert], GL_VERTEX_SHADER);
        shaders[frag] = ph::gl::compile_shader(path[frag], GL_FRAGMENT_SHADER);

        m_quad_program = glCreateProgram();

        ph::gl::link_program(m_quad_program, shaders, 2);

        ph_expect(Location_pos == glGetAttribLocation(m_quad_program, "position"));
        ph_expect(Location_tex == glGetUniformLocation(m_quad_program, "tex"));

        GLCHK ( glUseProgram(m_quad_program) );
        GLCHK ( glUniform1i(Location_tex, /*GL_TEXTURE_0*/0) );

    }
    // Create a quad.
    {
        glPointSize(3);
        const GLfloat u = 1.0f;
        GLfloat vert_data[] = {
            -u, u,
            -u, -u,
            u, -u,
            u, u,
        };
        glGenVertexArrays(1, &m_quad_vao);
        glBindVertexArray(m_quad_vao);

        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));

        GLCHK (glEnableVertexAttribArray (Location_pos) );
        GLCHK (glVertexAttribPointer     (/*attrib location*/Location_pos,
                    /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE, /*stride*/0, /*ptr*/0));
    }
    // Create compute shader
    {
        GLuint* shaders = phalloc(GLuint, num_shaders);
        for (int i = 0; i < num_shaders; ++i) {
            shaders[i] = gl::compile_shader(shader_paths[i], GL_COMPUTE_SHADER);
        }

        m_program = glCreateProgram();
        ph::gl::link_program(m_program, shaders, num_shaders);

        phree(shaders);

        ph_expect(Location_tex == glGetUniformLocation(m_program, "tex"));
        ph_expect(Location_screen_size == glGetUniformLocation(m_program, "screen_size"));

        glUseProgram(m_program);

        GLCHK ( glUniform1i(Location_tex, 0) );  // Location: 1, Texture Unit: 0
        float fsize[2] = {(float)width / 2, (float)height};
        glUniform2fv(Location_screen_size, 1, &fsize[0]);
    }

    // Safety net.
    static bool is_init = false;
    if (is_init) {
        phatal_error("vr::init called twice");
    }
    is_init = true;

    if (!ovr_Initialize()) {
        ph::phatal_error("Could not initialize OVR\n");
    }

    m_hmd = ovrHmd_Create(0);

    // TODO: avoid crash here by checking for ovrHmd_Create success.

    unsigned int sensor_caps =
        ovrTrackingCap_Position | ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection;

    ovrBool succ = ovrHmd_ConfigureTracking(m_hmd, sensor_caps, sensor_caps);
    if (!succ) {
        phatal_error("Could not initialize OVR sensors!");
    }

    auto fovPort_l = m_hmd->DefaultEyeFov[0];
    auto fovPort_r = m_hmd->DefaultEyeFov[1];

    // Hard coded, taken from OVR source.
    m_screen_size_m[0] = 0.12576f;
    m_screen_size_m[1] = 0.07074f;

    // Default fov (looking down)
    float hvfov = (fovPort_r.DownTan + fovPort_l.DownTan) / 2.0f;   // 1.32928
    float h = m_screen_size_m[1];
    h = 1;

    // TODO: take relief into account.
    m_default_eye_z = h / (1 * hvfov);


    const OVR::CAPI::HMDState* m_hmdstate = (OVR::CAPI::HMDState*)m_hmd->Handle;
    m_renderinfo = &m_hmdstate->RenderState.RenderInfo;
    m_hmdinfo    = &m_hmdstate->RenderState.OurHMDInfo;

    glUseProgram(m_program);
    glUniform1f(3, vr::m_default_eye_z);

    GLfloat size_m[2] = {
        m_screen_size_m[0] / 2,
        m_screen_size_m[1],
    };
    glUniform2fv(5, 1, size_m);     // screen_size_m
    glUniform1f(8, true);           // Cull?
}

void draw(int* resolution) {
    glUseProgram(m_program);
    GLfloat viewport_resolution[2] = { GLfloat(resolution[0] / 2), GLfloat(resolution[1]) };
    static unsigned int frame_index = 1;

    /* ovrFrameTiming frame_timing = ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index); */
    ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index);

    ovrPosef pose = ovrHmd_GetEyePose(vr::m_hmd, ovrEye_Left);

    // TODO -- Controls should be handled elsewhere
    auto q = pose.Orientation;
    GLfloat quat[4] {
        q.x, q.y, q.z, q.w,
    };
    auto p = pose.Position;
    GLfloat camera_pos[3];
    io::get_wasd_camera(quat, camera_pos);
    // add pos
    camera_pos[0] += p.x;
    camera_pos[1] += p.y;
    camera_pos[2] += p.z;

    GLCHK ( glUniform4fv(7, 1, quat) );  // Camera orientation
    GLCHK ( glUniform3fv(10, 1, camera_pos) );  // update camera_pos

    // Dispatch left viewport
    {
        GLfloat lens_center[2] = {
            (vr::m_screen_size_m[0] / 2) - (vr::m_renderinfo->LensSeparationInMeters / 2),
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        GLCHK ( glUniform1f(2, 0) );  // x_offset
        GLCHK ( glDispatchCompute(GLuint(viewport_resolution[0] / g_warpsize[0]),
                    GLuint(viewport_resolution[1] / g_warpsize[1]), 1) );
    }
    // Dispatch right viewport
    {
        GLfloat lens_center[2] = {
            vr::m_renderinfo->LensSeparationInMeters / 2,
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, ((GLfloat)viewport_resolution[0]));  // x_offset
        GLCHK ( glDispatchCompute(GLuint(viewport_resolution[0] / g_warpsize[0]),
                    GLuint(viewport_resolution[1] / g_warpsize[1]), 1) );
    }
    frame_index++;

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Fill screen
    {
        GLCHK (glUseProgram      (m_quad_program) );
        GLCHK (glBindVertexArray (m_quad_vao) );
        GLCHK (glDrawArrays      (GL_TRIANGLE_FAN, 0, 4) );
    }

    window::swap_buffers();
    glFlush();
    GLCHK ( glFinish() );
    ovrHmd_EndFrameTiming(m_hmd);

}

void deinit() {
    ovrHmd_Destroy(m_hmd);
    ovr_Shutdown();
}

}  // ns vr
}  // ns ph
