# == Project tardis

set (sources
    tardis.cc
    )

add_executable(tardis ${sources})
target_link_libraries(tardis ${nuwen_runtime} ovr)
set_property(TARGET tardis PROPERTY LINK_FLAGS "-lXrandr -ludev")
set_target_properties(tardis PROPERTIES COMPILE_DEFINITIONS "PH_OVR=1")
