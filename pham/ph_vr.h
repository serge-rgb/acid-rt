// 2014 Sergio Gonzalez

#pragma once

#include "ph_gl.h"

namespace ph {
namespace vr {

// Pass the width and height of the currently active GL context.
// Shaders must be compute shaders representing your app
// Required uniforms: (see test/compute.glsl)
//  tex: width x height sampler for rendering into and displaying.
//  screen_size
// ... the rest of the shaders should be concerned about writing
// interesting texels.
//
// Returns the compiled compute shader program so that you can set
// uniforms.
GLuint init(int width, int height, const char** shader_paths, int num_shaders);

// Works with ph::window
void draw();

} // ns vr
} // ns ph

