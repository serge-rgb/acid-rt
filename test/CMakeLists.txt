set(ph_sources
    "ph_test.cc")

add_executable(ph_test ${ph_sources})
target_link_libraries(ph_test ${nuwen_runtime})

set(compute_sources
    "compute.cc")
add_executable(compute ${compute_sources})
target_link_libraries(compute ${nuwen_runtime})
