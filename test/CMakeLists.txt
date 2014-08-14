# == ph
set(ph_sources
    "ph_test.cc")
add_executable(ph_test ${ph_sources})
target_link_libraries(ph_test ${nuwen_runtime})

# == compute
set(compute_sources
    "compute.cc")
add_executable(compute ${compute_sources})
target_link_libraries(compute ${nuwen_runtime})

# == cube grid

set (cube_sources
    ../pham/scene.cc
    cubes.cc)
add_executable(cubes ${cube_sources})
target_link_libraries(cubes ${nuwen_runtime})
# ==== Enable OVR compilation
# TODO: make this a function
set_target_properties(cubes PROPERTIES COMPILE_DEFINITIONS "PH_OVR=1")
target_link_libraries(cubes ${nuwen_runtime} ovr)
if (UNIX)
    set_property(TARGET cubes PROPERTY LINK_FLAGS "-lXrandr -ludev")
endif()

