include(../OVRLink.cmake)  # for ph_link_ovr

set (sample_sources
	${renderer_sources}
    samples.cc
    # == sponza
    #    sponza.cc
    # == bunny obj model
    #bunny.cc
    # == cube grid
    cubes.cc)

add_executable(samples ${sample_sources})
target_link_libraries(samples ${acid_runtime})
ph_link_ovr(samples)
