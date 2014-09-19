include(../OVRLink.cmake)  # for ph_link_ovr

set (sample_sources
	${engine_sources}
    samples.cc
# == cube grid
    cubes.cc)

add_executable(samples ${sample_sources})
target_link_libraries(samples ${nuwen_runtime})
ph_link_ovr(samples)
