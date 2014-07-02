// 2014 Sergio Gonzalez

#include <ph.h>

using namespace ph;

namespace vr {

/* static OVR::DeviceManager* m_manager; */
/* static OVR::HMDDevice* m_device; */
static const OVR::HMDInfo* m_hmdinfo;
static OVR::HmdRenderInfo* m_renderinfo;
static OVR::LensConfig m_lensconfig;
/* static OVR::DistortionRenderDesc* m_renderdesc_l; */
/* static OVR::DistortionRenderDesc* m_renderdesc_r; */
static ovrDistortionMesh* m_distmeshes[2];
static ovrHmd hmd;

void init() {

    // Allocate crap
    // Not deallocated. This should only be called once or the caller is dumb and ugly.
    /* m_hmdinfo = new OVR::HMDInfo; */
    m_renderinfo = new OVR::HmdRenderInfo();
    /* m_renderdesc_l = new OVR::DistortionRenderDesc(); */
    /* m_renderdesc_r = new OVR::DistortionRenderDesc(); */

    ///////////////

    if (!ovr_Initialize()) {
        ph::phatal_error("Could not initialize OVR\n");
    }
    hmd = ovrHmd_Create(0);
    ovrHmdDesc desc;
    ovrHmd_GetDesc(hmd, &desc);

    ovrHmd_StartSensor(hmd, 0x1111/*all*/, ovrHmd_GetEnabledCaps(hmd));

    auto fovPort_l = desc.DefaultEyeFov[0];
    auto fovPort_r = desc.DefaultEyeFov[1];

    // Down default fov
    float hvfov = (fovPort_r.DownTan + fovPort_r.DownTan) / 2.0f;

    printf("Default hvFov: %f\n", hvfov);

    // ovrEyeRenderDesc desc_l = ovrHmd_GetRenderDesc(hmd, ovrEye_Left, fovPort_l);
    // ovrEyeRenderDesc desc_r = ovrHmd_GetRenderDesc(hmd, ovrEye_Right, fovPort_r);


    ovrEyeRenderDesc rdesc[2];

    rdesc[0] = ovrHmd_GetRenderDesc(hmd, ovrEye_Left, fovPort_l);
    rdesc[1] = ovrHmd_GetRenderDesc(hmd, ovrEye_Right, fovPort_r);
    hmdState* state = (hmdState*)hmd;
    m_hmdinfo = &state->HMDInfo;
    float h = m_hmdinfo->ScreenSizeInMeters.h;
    printf("Physical height/2 %f\n", h/2);

    // Let's get the eye distance.
    float eye_z = h / (1 * hvfov);
    printf("eye z should be roughly %f\n", eye_z);

    *m_renderinfo = GenerateHmdRenderInfoFromHmdInfo(*m_hmdinfo);
    const auto dist_type = OVR::Distortion_RecipPoly4;
    m_lensconfig = OVR::GenerateLensConfigFromEyeRelief(0.01f, *m_renderinfo, dist_type);

    ovrHmd_CreateDistortionMesh(hmd, ovrEye_Left, fovPort_l, /*dist_caps? not used*/0, m_distmeshes[0]);
    ovrHmd_CreateDistortionMesh(hmd, ovrEye_Right, fovPort_r, /*dist_caps? not used*/0, m_distmeshes[1]);

    /*
    // Get stuff we need for ray tracing.
    //
    // Note: OVR says distortion in renderinfo is useless.
    // Presumably 0.01 meters is a good relief for DK1
    // Distortion is not deprecated but still simple to replicate in compute shader
    *m_renderdesc_l = OVR::CalculateDistortionRenderDesc(OVR::StereoEye_Left, *m_renderinfo, &m_lensconfig);
    *m_renderdesc_r = OVR::CalculateDistortionRenderDesc(OVR::StereoEye_Right, *m_renderinfo, &m_lensconfig);
    */
}



void deinit() {
    ovrHmd_StopSensor(hmd);
    ovrHmd_Destroy(hmd);
    ovr_Shutdown();
}

}

static GLuint g_program;
/* static int g_size[] = {1920, 1080}; */
static int g_size[] = {1280, 800};
// Note: perf is really sensitive about this. Runtime tweak?
static int g_warpsize[] = {8, 8};
static GLfloat m_viewport_size[2];

void init(GLuint prog) {
    // Eye-to-lens
    glUseProgram(prog);
    // Note... This hack is in LibOVR...
    // TODO: Check when Oculus does this without hacks...
    /* float eye_to_lens = vr::m_lensconfig.MetersPerTanAngleAtCenter; */
        /* 0.02733f +  // Screen center to midplate */
        /* vr::m_renderinfo->GetEyeCenter().ReliefInMeters + */
        /* vr::m_renderinfo->LensSurfaceToMidplateInMeters; */
    //float eye_to_lens = 0.021887f;
    float eye_to_lens = 0.07f;  // Not physically correct, TODO: check this
    glUniform1f(3, eye_to_lens);  // eye to lens.

    //screen_size.xy = vec2(0.149760, 0.93600);

    m_viewport_size[0] = GLfloat (g_size[0]) / 2;
    m_viewport_size[1] = GLfloat (g_size[1]);
    glUniform2fv(0, 1, m_viewport_size);  // screen_size
    GLfloat size_m[2] = {
        vr::m_renderinfo->ScreenSizeInMeters.w / 2,
        vr::m_renderinfo->ScreenSizeInMeters.h,
    };
    glUniform2fv(5, 1, size_m);  // screen_size in meters

    GLCHK();
}

void draw() {
    GLCHK ( glUseProgram(g_program) );

    static GLfloat sphere_y = 0.0f;
    static float step_var = 0.0;
    sphere_y = 0.2f * sinf(step_var);
    glUniform1f(4, sphere_y);
    step_var += 0.05;


    // Dispatch left viewport
    {
        GLfloat lens_center[2] = {
            (vr::m_renderinfo->ScreenSizeInMeters.w / 2) -
                (vr::m_renderinfo->LensSeparationInMeters / 2),
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, 0);  // x_offset
        GLCHK ( glDispatchCompute(GLuint(m_viewport_size[0] / g_warpsize[0]),
                    GLuint(m_viewport_size[1] / g_warpsize[1]), 1) );
    }
    // Dispatch right viewport
    {
        GLfloat lens_center[2] = {
            vr::m_renderinfo->LensSeparationInMeters / 2,
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, ((GLfloat)g_size[0] / 2.0f));  // x_offset
        GLCHK ( glDispatchCompute(GLuint(m_viewport_size[0] / g_warpsize[0]),
                    GLuint(m_viewport_size[1] / g_warpsize[1]), 1) );
    }

    // Draw screen
    cs::fill_screen();
}

int main() {
    vr::init();
    ph::init();

    window::init("Project TARDIS", g_size[0], g_size[1], window::InitFlag_no_decoration);

    const char* paths[] = {
        "tardis/main.glsl",
    };

    g_program = cs::init(g_size[0], g_size[1], paths, 1);

    init(g_program);

    window::draw_loop(draw);

    window::deinit();

    vr::deinit();
}

