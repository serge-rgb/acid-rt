include (../OVRLink.cmake)

set (sources
    ../pham/scene.cc
    chicken.cc)

add_executable(chicken ${sources})
target_link_libraries(chicken ${nuwen_runtime})
ph_link_ovr(chicken)


