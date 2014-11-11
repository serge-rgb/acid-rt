#include "ocl.h"

#include <ph.h>

#include <opencl.h>

#include "io.h"
#include "ph_gl.h"
#include "scene.h"  // Should not be necessary once this is a standalone module
#include "vr.h"
#include "window.h"

using namespace ph;


static const int width = 1920;
static const int height = 1080;


#ifdef OCL_MAIN
int main() {
    ph::ocl::init();

    ocl::CLtriangle tri;
    tri.p0[0] = 0;
    tri.p0[1] = 0;
    tri.p0[2] = -5;


    tri.p1[0] = 1;
    tri.p1[1] = 0;
    tri.p1[2] = -5;


    tri.p2[0] = 0;
    tri.p2[1] = 1;
    tri.p2[2] = -5;

    ph::ocl::set_triangle_soup(&tri, &tri, 1);

    window::main_loop(ocl::idle);

    ph::ocl::deinit();
    puts("Done.");
    return 0;
}
#endif

namespace ph {
namespace ocl {

static GLuint           m_gl_texture;
static GLuint           m_quad_vao;
static GLuint           m_quad_program;
static cl_context       m_context;
static cl_command_queue m_queue;
static cl_mem           m_cl_texture;
static cl_mem           m_cl_triangle_soup;
static cl_mem           m_cl_normal_soup;
static cl_program       m_cl_program;
static cl_kernel        m_cl_kernel;
static vr::HMDConsts    m_hmd_consts;

static int64 m_num_frames = 1;
static float m_avg_render = 0.0f;

void __stdcall context_callback(
        const char* errinfo, const void* /*private_info*/, size_t /*cb*/, void* /*user_data*/) {
    logf("OpenCL context error:  %s\n", errinfo);
}

void set_triangle_soup(scene::CLtriangle* tris, scene::CLtriangle* norms, size_t num_tris) {
    // If CL triangle soup doesn't exist, create
    static bool soup_exists = false;
    cl_int err = CL_SUCCESS;
    if (soup_exists) {
        phatal_error("Reset not implemented yet");
    }
    soup_exists = true;
    m_cl_triangle_soup = clCreateBuffer(m_context,
            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            12 * sizeof(float) * (size_t)num_tris, (void*)tris, &err);
    if (err != CL_SUCCESS) {
        phatal_error("Could not create buffer for tri soup");
    }
    m_cl_normal_soup = clCreateBuffer(m_context,
            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            12 * sizeof(float) * (size_t)num_tris, (void*)norms, &err);
    if (err != CL_SUCCESS) {
        phatal_error("Could not create buffer for normal soup");
    }

    // Set arguments.
    err = clSetKernelArg(m_cl_kernel,
            8, sizeof(cl_mem), (void*)&m_cl_triangle_soup);
    if (err != CL_SUCCESS) { phatal_error("Can't set kernel arg (tri soup)"); }

    err = clSetKernelArg(m_cl_kernel,
            9, sizeof(cl_mem), (void*)&m_cl_normal_soup);
    if (err != CL_SUCCESS) { phatal_error("Can't set kernel arg (normal soup)"); }

    err = clSetKernelArg(m_cl_kernel,
            10, sizeof(cl_int), (void*) &num_tris);
    if (err != CL_SUCCESS) { phatal_error("Can't set kernel arg (num_tris)"); }
}

void idle() {
    glFinish();

    vr::Eye left;
    vr::Eye right;
    vr::begin_frame(&left, &right);

    auto t_start = io::get_microseconds();

    cl_int err;

    err = clEnqueueAcquireGLObjects(
            m_queue, 1, &m_cl_texture, 0, NULL/*event wait list*/, NULL/*event*/);
    if (err != CL_SUCCESS) {
        phatal_error("Could not acquire texture from GL context");
    }

    cl_event event;  // TODO; I don't think I'm gonna need this...

    auto t_send = io::get_microseconds();

    size_t global_size[2] = {
        width/2,
        height,
    };

    size_t local_size[2] = {
        8,
        8,
    };

    // Kernel arguments that change every frame.
    cl_int off = 0;
    err = clSetKernelArg(m_cl_kernel,
            1, sizeof(cl_int), (void*) &off);
    err |= clSetKernelArg(m_cl_kernel,
            2, 2 * sizeof(float), (void*)&m_hmd_consts.lens_centers[vr::EYE_Left]);
    err |= clSetKernelArg(m_cl_kernel,
            6, sizeof(vr::Eye), (void*)&left);
    if (err != CL_SUCCESS) {
        phatal_error("Error setting kernel argument (left eye)");
    }

    // Left eye
    err = clEnqueueNDRangeKernel(
            m_queue,
            m_cl_kernel,
            2, //dim
            NULL, // offset
            global_size,
            local_size,
            0, NULL, &event);
    if (err != CL_SUCCESS) {
        phatal_error("Error enqueuing kernel (left)");
    }

    // Update parameters for right eye
    off = 960;
    err = clSetKernelArg(m_cl_kernel,
            1, sizeof(cl_int), (void*) &off);
    err |= clSetKernelArg(m_cl_kernel,
            2, 2 * sizeof(float), (void*)&m_hmd_consts.lens_centers[vr::EYE_Right]);
    err |= clSetKernelArg(m_cl_kernel,
            6, sizeof(vr::Eye), (void*)&right);
    if (err != CL_SUCCESS) {
        phatal_error("Error setting kernel argument (right eye)");
    }

    err = clEnqueueNDRangeKernel(
            m_queue,
            m_cl_kernel,
            2, //dim
            NULL, // offset
            global_size,
            local_size,
            0, NULL, &event);
    if (err != CL_SUCCESS) {
        phatal_error("Error enqueuing kernel (right)");
    }

    // Wait and release
    clFinish(m_queue);

    err = clEnqueueReleaseGLObjects(
            m_queue,
            1,
            &m_cl_texture,
            0, NULL, NULL);
    if (err != CL_SUCCESS) {
        phatal_error("could not release texture");
    }

    auto t_draw = io::get_microseconds();

    // Draw texture to screen
    {
        glUseProgram(m_quad_program);
        glBindTexture(GL_TEXTURE_2D, m_gl_texture);
        glBindVertexArray(m_quad_vao);
        GLCHK (glDrawArrays (GL_TRIANGLE_FAN, 0, 4) );
    }

    GLCHK ( glFinish() );

    auto t_end = io::get_microseconds();

    logf("Total frame time: %f\nOpenCL time: %f\nDraw time: %f\n=====================\n",
            float(t_end - t_start) / 1000.0f,
            float(t_draw - t_send) / 1000.0f,
            float(t_end - t_draw) / 1000.0f
            );

    m_avg_render += float(t_draw - t_send) / 1000.0f;
    m_num_frames++;

    window::swap_buffers();

    GLCHK ( glFinish() );

    vr::end_frame();
}

// For empty GL_CHECK().
static void no_op() {}

void init() {
    // ========================================
    // Get platforms
    // ========================================
    cl_uint num_platforms = 0;
    clGetPlatformIDs(0, NULL, &num_platforms);

    cl_platform_id *platforms = phalloc(cl_platform_id, num_platforms);
    clGetPlatformIDs(num_platforms, platforms, NULL);

    // TODO: check platform for respective GL interop extension.
    for (cl_uint i = 0; i < num_platforms; ++i) {
        ph::log("==== platform: ");
        cl_platform_info info_type[2] = {
            CL_PLATFORM_PROFILE,
            CL_PLATFORM_VERSION,
        };
        for (int j = 0; j < 2; ++j) {
            size_t sz = 0;
            clGetPlatformInfo(platforms[i], info_type[j], 0, NULL, &sz);
            char* str = phalloc(char, sz);
            clGetPlatformInfo(platforms[i], info_type[j], sz, (void*)str, NULL);
            ph::log(str);
            phree(str);
        }
        ph::log("================\n\n");
    }

    if (num_platforms == 0) {
        ph::phatal_error("No OpenCL platforms found.");
    }

    ph::log("Using first platform.\n");

    cl_uint num_devices = 0;
    clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);

    if (num_devices == 0) {
        phatal_error("Did not find OpenCL devices.");
    }

    cl_device_id* devices = phalloc(cl_device_id, num_devices);
    clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, num_devices, devices, &num_devices);

    for (cl_uint i = 0; i < num_devices; ++i) {
        log("==== OpenCL Device:");
        cl_device_info infos[] = {
            CL_DEVICE_NAME,
            CL_DEVICE_VENDOR,
            CL_DEVICE_VERSION,
        };
        for (int j = 0; j < sizeof(infos) / sizeof(cl_device_info); j++) {
            size_t sz = 0;
            clGetDeviceInfo(devices[i], infos[j], 0, NULL, &sz);
            char* value = phalloc(char, sz);

            clGetDeviceInfo(devices[i], infos[j], sz, (void*)value, NULL);
            log(value);
            phree(value);
        }
        ph::log("================\n\n");
    }

    log("Using first device\n");

    cl_device_id device = devices[0];
    cl_int err = 0;

    cl_context_properties* props = NULL;

    // ========================================
    // Platform specific context creation w/OpenGL ctx sharing
    // ========================================
#if defined (__APPLE__)
    CGLContextObj kCGLContext = CGLGetCurrentContext();
    CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
    cl_context_properties local_props[] =
    {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)kCGLShareGroup,
        0
    };
    props = local_props;
#else
#ifdef UNIX
    cl_context_properties local_props[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
        CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
        //CL_CONTEXT_PLATFORM, (cl_context_properties)cpPlatform,
        0
    };
    props = local_props;
#else // Win32
    cl_context_properties local_props[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
        0
    };
    props = local_props;
#endif
#endif
    m_context = clCreateContext(
            props,
            1, &devices[0],
            context_callback,
            /*user_data=*/NULL,
            &err);

    if (err != CL_SUCCESS) {
        logf("err num %d\n", err);
        phatal_error("Cannot create context");
    }

    // ========================================
    // Command queue
    // ========================================

    m_queue = clCreateCommandQueue(m_context, device, 0, &err);
    if (err != CL_SUCCESS) {
        logf("err num %d\n", err);
        phatal_error("Cannot create command queue");
    }

    // ========================================
    // Do stuff before we block and go to main loop
    // ========================================

    m_hmd_consts = vr::get_hmd_constants();

    // Create GL "framebufer".
    {
        // Create texture
        GLCHK (glActiveTexture (GL_TEXTURE0) );
        glGenTextures   (1, &m_gl_texture);
        glBindTexture   (GL_TEXTURE_2D, m_gl_texture);

        // Note for the future: These are needed.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Pass a null pointer, texture will be filled by opencl ray tracer
        GLCHK ( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height,
                    0, GL_RGBA, GL_FLOAT, NULL) );

        GLCHK ( no_op() );
    }

    // Create GL quad program to fill screen.
    {
        // Create the quad
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

            GLCHK (glEnableVertexAttribArray (0) );
            GLCHK (glVertexAttribPointer     (/*attrib location*/0,
                        /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE, /*stride*/0, /*ptr*/0));
        }
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

        m_quad_program = glCreateProgram();
        ph::gl::link_program(m_quad_program, shaders, shader_count);

        GLCHK ( glUseProgram(m_quad_program) );
        glUniform1i(1, /*GL_TEXTURE_0*/0);
        GLfloat rcp_frame[2] = { 1.0f/(float)width, 1.0f/(float)height };
        glUniform2fv(2, 1, rcp_frame);
        GLfloat size [2] = { GLfloat(width), GLfloat(height) };
        glUniform2fv(3, 1, size);

        glUniform2fv(4, 1, m_hmd_consts.lens_centers[vr::EYE_Left]);
        glUniform2fv(5, 1,  m_hmd_consts.lens_centers[vr::EYE_Right]);
    }

    // Create OpenCL buffer
    {
        cl_mem_flags flags = CL_MEM_WRITE_ONLY;
        m_cl_texture = clCreateFromGLTexture2D(
                m_context,
                flags,
                /*texture_target*/GL_TEXTURE_2D,
                /*miplevel*/0,
                m_gl_texture,
                &err);
        if (err != CL_SUCCESS) {
            switch (err) {
            case CL_INVALID_CONTEXT:
                log("CL_INVALID_CONTEXT");
                break;
            case CL_INVALID_VALUE:
                log("CL_INVALID_VALUE");
                break;
            default:
                log("???");
            }
            phatal_error("Could not create OpenCL image from GL texture");
        }
    }

    // Create program & kernel.
    {
        static const char* path = "src/tracer.cl";
        auto source = io::slurp(path);
        m_cl_program = clCreateProgramWithSource(
                m_context,
                1,
                &source,
                NULL, /*lengths, NULL means lines end in \0*/
                &err
                );
        if (err != CL_SUCCESS) {
            logf("could not create program from source %s", path);
            ph::quit(EXIT_FAILURE);
        }
        // Build program
        const char* options = "";  // 5.6.3.3 p117
        err = clBuildProgram(
                m_cl_program,
                1,
                &device,
                options,
                NULL, // callback
                NULL  // user data
                );
        if (err != CL_SUCCESS) {
            // Write build result.
            size_t sz;
            clGetProgramBuildInfo(
                    m_cl_program,
                    device,
                    CL_PROGRAM_BUILD_LOG,
                    0, NULL, &sz);

            char* log = phanaged(char, sz);

            clGetProgramBuildInfo(
                    m_cl_program,
                    device,
                    CL_PROGRAM_BUILD_LOG,
                    sz, (void*)log, 0);
            puts(log);
            phatal_error("Could not build program");
        }
    }
    {  // Get kernel
        m_cl_kernel = clCreateKernel(m_cl_program, "main", &err);
        if (err != CL_SUCCESS) {
            phatal_error("Can't get kernel from program.");
        }
        // Set argument to be the texture.
        if (err != CL_SUCCESS) {
            switch(err) {
            case CL_INVALID_SAMPLER:
                log("invalid sampler");
                break;
            case CL_INVALID_KERNEL:
                log("invalid kernel");
                break;
            case CL_INVALID_ARG_INDEX:
                log("invalid arg index");
                break;
            case CL_INVALID_MEM_OBJECT:
                log("invalid mem object");
                break;
            default:
                log("???");
                break;
            }
            phatal_error("Can't set kernel image param.");
        }
    }

    // Distortion coefficients
    float K[11];
    vr::fill_catmull_K(K, 11);
    // Create / fill CL buffer...
    cl_mem cl_K;
    {
        cl_K = clCreateBuffer(m_context,
                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 11 * sizeof(float), (void*)K, &err);
        if (err != CL_SUCCESS) {
            phatal_error("Can't create K CL buffer");
        }
        for (int i = 0; i < 11; ++i) {
            logf("K[%d] : %f\n", i, K[i]);
        }
    }


    // Set arguments to the kernel that don't change per frame.
    err = clSetKernelArg(m_cl_kernel,
            0, sizeof(cl_mem), (void*) &m_cl_texture);
    err |= clSetKernelArg(m_cl_kernel,
            3, sizeof(float), (void*)&m_hmd_consts.eye_to_screen);
    err |= clSetKernelArg(m_cl_kernel,
            4, 2 * sizeof(float), (void*)&m_hmd_consts.viewport_size_m);
    int size_px[2] = { width / 2, height / 2 };
    err |= clSetKernelArg(m_cl_kernel,
            5, 2 * sizeof(int), (void*)size_px);
    err |= clSetKernelArg(m_cl_kernel,
            7, sizeof(cl_mem), (void*)&cl_K);
    if (m_hmd_consts.meters_per_tan_angle != 0.036f) {
        logf("MetersPerTanAngleAtCenter is %f, expected 0.036\n", m_hmd_consts.meters_per_tan_angle);
        phatal_error("Exiting");
    }


    if (err != CL_SUCCESS) {
        phatal_error("Some kernel argument was not set at ocl init.");
    }

    log("HMD Info: =======");
    float xl = m_hmd_consts.lens_centers[vr::EYE_Left][0];
    float yl = m_hmd_consts.lens_centers[vr::EYE_Left][1];
    float xr = m_hmd_consts.lens_centers[vr::EYE_Right][0];
    float yr = m_hmd_consts.lens_centers[vr::EYE_Right][1];

    logf("Eye centers: Left (%f, %f) , Right (%f, %f)\n",
            xl, yl, xr, yr);
    logf("Eye to screen dist: %f\n",
            m_hmd_consts.eye_to_screen);
    logf("Screen size meters: (%f, %f)\n",
            2 * m_hmd_consts.viewport_size_m[0], m_hmd_consts.viewport_size_m[1]);

    phree(platforms);
    phree(devices);
}

void deinit() {
    clReleaseProgram(m_cl_program);
    clReleaseCommandQueue(m_queue);
    clReleaseContext(m_context);

    window::deinit();

    logf("average opencl time is %f(%d)\n", m_avg_render / (float)m_num_frames, m_num_frames);
}

}  // ns ocl
}  // ns ph
