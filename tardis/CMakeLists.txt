# == Project tardis

set (sources
    tardis.cc
    # Higher level
    ../pham/scene.cc
    )

add_executable(tardis ${sources})
# ==== Enable OVR compilation
# TODO: make this a function
set_target_properties(tardis PROPERTIES COMPILE_DEFINITIONS "PH_OVR=1")
target_link_libraries(tardis ${nuwen_runtime} ovr)
if (UNIX)
    set_property(TARGET tardis PROPERTY LINK_FLAGS "-lXrandr -ludev")
endif()
