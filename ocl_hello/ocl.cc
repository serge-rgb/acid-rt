#include <ph.h>

#include <opencl.h>

#include "io.h"
#include "ph_gl.h"
#include "window.h"

using namespace ph;


static const int width = 1920;
static const int height = 1080;

static GLuint m_gl_texture;
static GLuint m_quad_program;
static cl_mem m_cl_texture;


void __stdcall context_callback(
        const char* errinfo, const void* /*private_info*/, size_t /*cb*/, void* /*user_data*/) {
    logf("OpenCL context error:  %s\n", errinfo);
}

void ocl_idle() {
    // Run OpenCL kernel

    // Wait and release

    // Draw texture to screen

    window::swap_buffers();
}

// For empty GL_CHECK().
static void no_op() {}

int main() {
    ph::init();

    // Creates a GL context, so we need to init the window before doing OpenCL.
    window::init("OCL", width, height, window::InitFlag_Default);

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
    //context = clCreateContext(local_props, 0,0, NULL, NULL, &ciErrNum);
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
    //context = clCreateContext(local_props, 1, , NULL, NULL, &ciErrNum);
    props = local_props;
#else // Win32
    cl_context_properties local_props[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
        0
    };
    //context = clCreateContext(local_props, 1, &cdDevices[uiDeviceUsed], NULL, NULL, &ciErrNum);
    props = local_props;
#endif
#endif
    cl_context context = clCreateContext(
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
    // ========================================

    // ========================================
    // Command queue
    // ========================================

    cl_command_queue queue =
        clCreateCommandQueue(context, device, 0, &err);
        //clCreateCommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
    // ^----- commands can be profiled (spec: 5.12)
    if (err != CL_SUCCESS) {
        logf("err num %d\n", err);
        phatal_error("Cannot create command queue");
    }


    // ========================================
    // Do stuff before we block and go to main loop
    // ========================================

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

        enum {
            Location_pos = 0,
            Location_tex = 1,
        };
        ph_expect(Location_pos == glGetAttribLocation(m_quad_program, "position"));
        ph_expect(Location_tex == glGetUniformLocation(m_quad_program, "tex"));

        GLCHK ( glUseProgram(m_quad_program) );
        GLCHK ( glUniform1i(Location_tex, /*GL_TEXTURE_0*/0) );
    }

    // Create buffer
    {
        cl_mem_flags flags = CL_MEM_WRITE_ONLY;
        m_cl_texture = clCreateFromGLTexture2D(
                context,
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

    cl_program m_cl_program;
    // Create program & kernel.
    {
        static const char* path = "ocl_hello/checker.cl";
        auto source = io::slurp(path);
        puts(source);
        m_cl_program = clCreateProgramWithSource(
                context,
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
            phatal_error("Could not build program");
        }
    }

    // ========================================
    window::main_loop(ocl_idle);
    // ========================================


    clReleaseProgram(m_cl_program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    phree(platforms);
    phree(devices);

    window::deinit();

    puts("Done.");
    return 0;
}
