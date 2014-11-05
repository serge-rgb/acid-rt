include(../OVRLink.cmake)  # for ph_link_ovr

# == ocl_hello
set (cl_sources
    "ocl.cc"
    ${renderer_sources})
add_executable(ocl_hello ${cl_sources})
target_link_libraries(ocl_hello ${acid_runtime})
ph_link_ovr(ocl_hello)
