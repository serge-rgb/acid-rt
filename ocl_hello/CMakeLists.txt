# == ocl_hello
set (cl_sources
    "ocl.cc")
add_executable(ocl_hello ${cl_sources})
target_link_libraries(ocl_hello ${acid_runtime})
