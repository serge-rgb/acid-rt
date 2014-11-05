#include <ph.h>

#include <opencl.h>

using namespace ph;

void __stdcall context_callback(
        const char* errinfo, const void* /*private_info*/, size_t /*cb*/, void* /*user_data*/) {
    logf("OpenCL context error:  %s\n", errinfo);
}

int main() {
    ph::init();

    // ========================================
    // Get platforms
    // ========================================
    cl_uint num_platforms = 0;
    clGetPlatformIDs(0, NULL, &num_platforms);

    cl_platform_id *platforms = phalloc(cl_platform_id, num_platforms);
    clGetPlatformIDs(num_platforms, platforms, NULL);

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

    cl_context context = clCreateContext(
            /*properties=*/NULL,
            1, &devices[0],
            context_callback,
            /*user_data=*/NULL,
            &err);

    if (err != CL_SUCCESS) {
        logf("err num %d\n", err);
        phatal_error("Cannot create command queue");
    }

    // ========================================
    // Command queue
    // ========================================

    cl_command_queue queue = clCreateCommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
    // ^----- commands can be profiled (spec: 5.12)

    if (err != CL_SUCCESS) {
        logf("err num %d\n", err);
        phatal_error("Cannot create context");
    }

    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    phree(platforms);
    phree(devices);

    puts("Done.");
    return 0;
}
