set(sources
    "ph_test.cc")

add_executable(ph_test ${sources})
target_link_libraries(ph_test ${nuwen_runtime})
