# == Project tardis

set (sources
    tardis.cc
    )

add_executable(tardis ${sources})
target_link_libraries(tardis ${nuwen_runtime})
