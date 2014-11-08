#include "vr.h"

#include "ph_gl.h"
#include "window.h"

#define PH_DEBUG_FRAMETIME

// Note 2: Workgroup size should be a multiple of workgroup size.
static int g_warpsize[] = {10, 16};
//static int g_warpsize[] = {1, 256};  // Nice trade-off between perf and artifacts.

namespace ph {
namespace vr {

static bool                      m_has_initted = false;

static GLuint                    m_quad_vao;
static GLuint                    m_quad_program;
static GLuint                    m_compute_program;

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
static bool                      m_do_interlace_throttling = false;
static bool                      m_skybox_enabled = true;
static ovrEyeRenderDesc          m_render_desc_l;
static ovrEyeRenderDesc          m_render_desc_r;
static unsigned int              m_frame_index = 0;

ovrHmd                           m_hmd;

vr::HMDConsts                    m_cached_consts;

#ifndef GL_DEPRECATED
void init(int width, int height) {
    const char* paths[] = {
        "src/tracing.glsl",
    };
    init_with_shaders(width, height, paths, 1);
}

void init_with_shaders(int width, int height, const char** shader_paths, int num_shaders) {
#else
void init() {
#endif
    // Safety net.
    if (m_has_initted) {
        phatal_error("vr::init called twice");
    }
    m_has_initted = true;

#ifndef GL_DEPRECATED
    enum Location {
        Location_pos = 0,
        Location_screen_size = 0,
        Location_tex = 1,
    };

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

        glUniform1i(12, false);  // interlacing
        glUniform1i(13, /*GL_TEXTURE2*/2);  // skybox
        glUniform1i(14, m_skybox_enabled);
        GLCHK ( glUniform1i(Location_tex, 0) );  // Location: 1, Texture Unit: 0
        GLCHK ( glUniform1i(11, 1) );  // Back texture

        float fsize[2] = {(float)width / 2, (float)height};
        glUniform2fv(Location_screen_size, 1, &fsize[0]);
    }
#endif // GL_DEPRECATED

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

    m_render_desc_l = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Left, m_hmd->DefaultEyeFov[0]);
    m_render_desc_r = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Right, m_hmd->DefaultEyeFov[1]);
    /* auto lens_config = GenerateLensConfigFromEyeRelief(0.008f, *m_renderinfo); */

    /* for (int i = 0; i < OVR::LensConfig::NumCoefficients; ++i) { */
    /*     printf("K[%d] = %f\n", i, lens_config.K[i]); */
    /* } */

    //////////////////////////////////////////////
    // Set up our cached constants
    //////////////////////////////////////////////

    memcpy(m_cached_consts.lens_centers[EYE_Left], m_lens_center_l, 2 * sizeof(float));
    memcpy(m_cached_consts.lens_centers[EYE_Right], m_lens_center_r, 2 * sizeof(float));

    m_cached_consts.eye_to_screen = m_default_eye_z;

    m_cached_consts.viewport_size_m[0] = m_screen_size_m[0] / 2;
    m_cached_consts.viewport_size_m[1] = m_screen_size_m[1];


#ifndef GL_DEPRECATED
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
#endif
}

const HMDConsts get_hmd_constants() {
    if (!m_has_initted) {
        phatal_error("Trying to get hmd info without initting module");
    }
    return m_cached_consts;
}

void toggle_postproc() {
    m_do_postprocessing = m_do_postprocessing? false : true;
}

void toggle_interlace_throttle() {
    m_do_interlace_throttling = m_do_interlace_throttling? false : true;
}

void enable_skybox() {
    m_skybox_enabled = true;
    glUseProgram(m_program);
    glUniform1i(14, m_skybox_enabled);
}

void disable_skybox() {
    m_skybox_enabled = false;
    glUseProgram(m_program);
    glUniform1i(14, m_skybox_enabled);
}

void begin_frame(Eye* left, Eye* right) {
    ph_assert(left != NULL);
    ph_assert(right != NULL);
    ovrHmd_BeginFrameTiming(m_hmd, m_frame_index);

    ovrPosef poses[2];
    {
        ovrVector3f offsets[2] = {
            m_render_desc_l.HmdToEyeViewOffset,
            m_render_desc_r.HmdToEyeViewOffset
        };
        ovrHmd_GetEyePoses(m_hmd, 0, offsets, poses, /*ovrTrackingState*/NULL);
    }

    Eye* eyes[2] = { left, right };

    for (int i = 0; i < EYE_Count; ++i) {
        ovrPosef pose = poses[i];
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

        eyes[i]->position[0] = camera_pos[0];
        eyes[i]->position[1] = camera_pos[1];
        eyes[i]->position[2] = camera_pos[2];
        eyes[i]->orientation[0] = quat[0];
        eyes[i]->orientation[1] = quat[1];
        eyes[i]->orientation[2] = quat[2];
        eyes[i]->orientation[3] = quat[3];
    }
}

void end_frame() {
    ovrHmd_EndFrameTiming(m_hmd);
    m_frame_index++;
}
#ifndef GL_DEPRECATED
void draw(int* resolution) {
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(m_program);
    GLfloat viewport_resolution[2] = { GLfloat(resolution[0] / 2), GLfloat(resolution[1]) };
    static unsigned int frame_index = 1;

    /* ovrFrameTiming frame_timing = ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index); */
    ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index);
    long frame_begin = io::get_microseconds();

    ovrPosef poses[2];
    {
        ovrVector3f offsets[2] = {
            m_render_desc_l.HmdToEyeViewOffset,
            m_render_desc_r.HmdToEyeViewOffset
        };
        ovrHmd_GetEyePoses(m_hmd, 0, offsets, poses, /*ovrTrackingState*/NULL);
    }


    // TODO -- Controls should be handled elsewhere
    auto update_camera = [&poses](int i) {
        ovrPosef pose = poses[i];
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
    };

    GLCHK ( glUniform1i(9, (GLint)frame_index) );
    // Dispatch left viewport
    {
        update_camera(0);
        glUniform2fv(6, 1, m_lens_center_l);  // Lens center
        GLCHK ( glUniform1f(2, 0) );  // x_offset
        GLCHK ( glDispatchCompute(GLuint(viewport_resolution[0] / g_warpsize[0]),
                    GLuint(viewport_resolution[1] / g_warpsize[1]), 1) );
    }
    // Dispatch right viewport
    {
        update_camera(1);
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

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    GLCHK ( glFinish() );
    long frame_end = io::get_microseconds();
    double frame_time_ms = (frame_end - frame_begin) / 1000.0; // Before vsync
    glUseProgram(m_program);
    if (m_do_interlace_throttling) {
        if (frame_time_ms > 13.333333) {
            glUniform1i(12, true);  // interlacing
        }
        if (frame_time_ms < 0.5 * 13.333333) {
            glUniform1i(12, false);  // interlacing
        }
    } else {
        glUniform1i(12, false);  // interlacing
    }
#ifdef PH_DEBUG_FRAMETIME
    printf("Frame time is %f\n", frame_time_ms);
#endif
    ovrHmd_EndFrameTiming(m_hmd);
    window::swap_buffers();

}
#endif //GL_DEPRECATED

void deinit() {
    ovrHmd_Destroy(m_hmd);
    ovr_Shutdown();
}

}  // ns vr
}  // ns ph
