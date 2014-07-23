// 2014 Sergio Gonzalez

#pragma once

#include "ph_gl.h"

namespace ph {

////////////////////////////////////////
//          -- ph::cs --
//  Create a GLSL compute program for
//  rendering..
////////////////////////////////////////
namespace cs {

// Pass the width and height of the currently active GL context.
// Shaders must be compute shaders representing your app
// Required uniforms: (see test/compute.glsl)
//  tex: width x height sampler for rendering into and displaying.
//  screen_size
// ... the rest of the shaders should be concerned about writing
// interesting texels.
//
// Returns the compiled compute shader program.
GLuint init(int width, int height, const char** shader_paths, int num_shaders);

// Usage:
//     Dispatch, set your memory barriers, and then call cs::fill_screen()
//     to fill the screen with the texture shared with the compute shader.
void fill_screen();

} // ns cs
} // ns ph

