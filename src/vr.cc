#include "vr.h"


// Note 2: Workgroup size should be a multiple of workgroup size.
//static int g_warpsize[] = {2, 32};
//static int g_warpsize[] = {4, 16}; // Good perf.
static int g_warpsize[] = {1, 128};  // Nice trade-off between perf and artifacts.

namespace ph {
namespace vr {

static GLuint                    m_quad_vao;
static GLuint                    m_quad_program;
static GLuint                    m_compute_program;
static GLuint                    m_size[2];             // Size of the framebuffer
static ovrHmd                    m_hmd;
static float                     m_default_eye_z;       // Eye distance from plane.
static const OVR::HMDInfo*       m_hmdinfo;
static const OVR::HmdRenderInfo* m_renderinfo;
static float                     m_screen_size_m[2];    // Screen size in meters
static GLuint                    m_program = 0;
static GLuint                    m_postprocess_program = 0;
static GLuint                    m_screen_tex;
static GLuint                    m_backbuffer_tex;
static float                     m_lens_center_l[2];
static float                     m_lens_center_r[2];
static bool                      m_do_postprocessing = true;
static bool                      m_do_interlacing = false;


void init(int width, int height) {
    const char* paths[] = {
        "src/tracing.glsl",
    };
    init_with_shaders(width, height, paths, 1);
}

void init_with_shaders(int width, int height, const char** shader_paths, int num_shaders) {
    // Safety net.
    static bool is_init = false;
    if (is_init) {
        phatal_error("vr::init called twice");
    }
    is_init = true;

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
        GLCHK (glActiveTexture (GL_TEXTURE0) );
        GLCHK (glGenTextures   (1, &m_screen_tex) );
        GLCHK (glBindTexture   (GL_TEXTURE_2D, m_screen_tex) );

        // Note for the future: These are needed.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Pass a null pointer, texture will be filled by compute shader
        // Note: Internal format was vital for compute shader.
        GLCHK ( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height,
                    0, GL_RGBA, GL_FLOAT, NULL) );

        // Bind it to image unit
        /* GLCHK ( glBindImageTexture(0, m_screen_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F) ); */
        GLCHK ( glBindImageTexture(0, m_screen_tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F) );


        // Make a second image, dual buffering (for checkerboard interlacing).
        glActiveTexture (GL_TEXTURE1);
        glGenTextures   (1, &m_backbuffer_tex);
        glBindTexture   (GL_TEXTURE_2D, m_backbuffer_tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLCHK ( glBindImageTexture(1, m_backbuffer_tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F) );

        glActiveTexture(GL_TEXTURE0);

    }
    // Create main program
    {
        enum {
            vert,
            frag,
            shader_count,
        };
        GLuint shaders[shader_count];
        const char* path[shader_count];
        path[vert] = "src/quad.v.glsl";
        path[frag] = "src/quad.f.glsl";
        shaders[vert] = ph::gl::compile_shader(path[vert], GL_VERTEX_SHADER);
        shaders[frag] = ph::gl::compile_shader(path[frag], GL_FRAGMENT_SHADER);

        m_quad_program = glCreateProgram();

        ph::gl::link_program(m_quad_program, shaders, shader_count);

        ph_expect(Location_pos == glGetAttribLocation(m_quad_program, "position"));
        ph_expect(Location_tex == glGetUniformLocation(m_quad_program, "tex"));

        GLCHK ( glUseProgram(m_quad_program) );
        GLCHK ( glUniform1i(Location_tex, /*GL_TEXTURE_0*/0) );
    }
    // Create post pocessing program.
    {
        enum {
            vert,
            frag,
            fxaa,
            shader_count,
        };
        GLuint shaders[shader_count];
        const char* paths[shader_count];
        paths[vert] = "src/quad.v.glsl";
        paths[frag] = "src/postproc.f.glsl";
        paths[fxaa] = "third_party/src/Fxaa3_11.glsl";

        shaders[vert] = ph::gl::compile_shader(paths[vert], GL_VERTEX_SHADER);
        shaders[frag] = ph::gl::compile_shader(paths[frag], GL_FRAGMENT_SHADER);
        shaders[fxaa] = ph::gl::compile_shader(paths[fxaa], GL_FRAGMENT_SHADER);

        m_postprocess_program = glCreateProgram();
        ph::gl::link_program(m_postprocess_program, shaders, shader_count);

        glUseProgram(m_postprocess_program);
        ph_expect(Location_pos == glGetAttribLocation(m_postprocess_program, "position"));
        ph_expect(1 == glGetUniformLocation(m_postprocess_program, "fbo_texture"));
        ph_expect(2 == glGetUniformLocation(m_postprocess_program, "rcp_frame"));
        glUniform1i(1, /*GL_TEXTURE*/0);  // Get color from the ray traced texture.
        float fsize[2] = { 1.0f/((float)width), 1.0f/((float)height) };
        GLCHK ( glUniform2fv(2, 1, &fsize[0]) );
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

        glUniform1i(12, m_do_interlacing);
        GLCHK ( glUniform1i(Location_tex, 0) );  // Location: 1, Texture Unit: 0
        GLCHK ( glUniform1i(11, 1) );  // Back texture

        float fsize[2] = {(float)width / 2, (float)height};
        glUniform2fv(Location_screen_size, 1, &fsize[0]);
    }

    if (!ovr_Initialize()) {
        ph::phatal_error("Could not initialize OVR\n");
    }

    m_hmd = ovrHmd_Create(0);

    unsigned int sensor_caps =
        ovrTrackingCap_Position | ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection;

    ovrBool succ = ovrHmd_ConfigureTracking(m_hmd, sensor_caps, sensor_caps);
    if (!succ) {
        phatal_error("Could not initialize OVR sensors!");
    }

    auto fovPort_l = m_hmd->DefaultEyeFov[0];

    // TODO: take relief into account.
    float theta = atanf(fovPort_l.DownTan);
    m_default_eye_z =  2 * abs(cosf(2 * theta));

    // Hard coded, taken from OVR source.
    m_screen_size_m[0] = 0.12576f;
    m_screen_size_m[1] = 0.07074f;

    const OVR::CAPI::HMDState* m_hmdstate = (OVR::CAPI::HMDState*)m_hmd->Handle;
    m_renderinfo                          = &m_hmdstate->RenderState.RenderInfo;
    m_hmdinfo                             = &m_hmdstate->RenderState.OurHMDInfo;

    // TODO: add this to shader.
    // float t = m_renderinfo->EyeLeft.Distortion.MetersPerTanAngleAtCenter;

    m_lens_center_l[0] = (vr::m_screen_size_m[0] / 2) - (vr::m_renderinfo->LensSeparationInMeters / 2);
    m_lens_center_l[1] = vr::m_hmdinfo->CenterFromTopInMeters;

    m_lens_center_r[0] = vr::m_renderinfo->LensSeparationInMeters / 2;
    m_lens_center_r[1] = vr::m_hmdinfo->CenterFromTopInMeters;

    // Get as much from the OVR config util as possible. (i.e. some unnecessary work done by libOVR)
    auto render_desc = CalculateDistortionRenderDesc(OVR::StereoEye(OVR::StereoEye_Left), *m_renderinfo);
    /* auto lens_config = GenerateLensConfigFromEyeRelief(0.008f, *m_renderinfo); */

    /* for (int i = 0; i < OVR::LensConfig::NumCoefficients; ++i) { */
    /*     printf("K[%d] = %f\n", i, lens_config.K[i]); */
    /* } */

    GLfloat size_m[2] = {
        m_screen_size_m[0] / 2,
        m_screen_size_m[1],
    };

    glUseProgram(m_postprocess_program);

    glUniform2fv(3, 1, size_m);     // screen_size_m
    glUniform2fv(4, 1, m_lens_center_l);
    glUniform2fv(5, 1, m_lens_center_r);

    glUseProgram(m_program);
    glUniform1f(3, vr::m_default_eye_z);

    glUniform2fv(5, 1, size_m);     // screen_size_m
    glUniform1f(8, true);           // Cull?

    // TODO: is this necesary for extended mode?
    // ovrHmd_AttachToWindow(m_hmd, window::m_window, NULL, NULL);
}

void toggle_postproc() {
    m_do_postprocessing = m_do_postprocessing? false : true;
}

void toggle_interlacing() {
    m_do_interlacing = m_do_interlacing? false : true;
    glUseProgram(m_program);
    GLCHK ( glUniform1i(12, m_do_interlacing) );
}

void draw(int* resolution) {
    glActiveTexture(GL_TEXTURE0);
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

    GLCHK ( glUniform1i(9, (GLint)frame_index) );
    // Dispatch left viewport
    {
        glUniform2fv(6, 1, m_lens_center_l);  // Lens center
        GLCHK ( glUniform1f(2, 0) );  // x_offset
        GLCHK ( glDispatchCompute(GLuint(viewport_resolution[0] / g_warpsize[0]),
                    GLuint(viewport_resolution[1] / g_warpsize[1]), 1) );
    }
    // Dispatch right viewport
    {
        glUniform2fv(6, 1, m_lens_center_r);  // Lens center
        glUniform1f(2, ((GLfloat)viewport_resolution[0]));  // x_offset
        GLCHK ( glDispatchCompute(GLuint(viewport_resolution[0] / g_warpsize[0]),
                    GLuint(viewport_resolution[1] / g_warpsize[1]), 1) );
    }
    frame_index++;

    GLCHK ( glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) );
    //GLCHK ( glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT) );

    //////////////////////////////////////////////////////////////

    // Fill screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    {
        if (m_do_postprocessing) {
            glUseProgram(m_postprocess_program);
        } else {
            glUseProgram(m_quad_program);
        }
        GLCHK (glBindVertexArray (m_quad_vao) );
        GLCHK (glDrawArrays      (GL_TRIANGLE_FAN, 0, 4) );
    }

    glActiveTexture(GL_TEXTURE1);
    // Copy to back buffer
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,0,0, (GLsizei)m_size[0], (GLsizei)m_size[1], 0);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    window::swap_buffers();
    /* glFlush(); */
    /* GLCHK ( glFinish() ); */
    ovrHmd_EndFrameTiming(m_hmd);

}

void deinit() {
    ovrHmd_Destroy(m_hmd);
    ovr_Shutdown();
}

}  // ns vr
}  // ns ph
