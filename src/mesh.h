#pragma once

#include <ph.h>

#include "scene.h"

////////////////////////////////////////
// ph::mesh
// Load triangle models from file and
// set them up to be consumed by
// ph::scene
// Usage example:
//
//  auto big_chunk = load_obj("my_model.obj", 1.0);
//  auto small_chunks = shatter(big_chunk, 100);
//  for (int64 i = 0; i < count(small_chunks); ++i) {
//      scene::submit_primitive(&small_chunks[i], scene::SubmitFlags_None);
//  }
////////////////////////////////////////

namespace ph {
namespace mesh {

/**
 * Returns a triangle soup of the OBJ model specified at "path"
 */
scene::Chunk load_obj(const char* path, float scale);

/**
 * flags specifies the way faces are described. i.e.
 *  In OBJ
 * - LoadFlags_NoTexcoords
 * "f %d//%d %d//%d %d//%d"
 *
 * - LoadFlags_Default
 * "f %d/%d/%d %d/%d/%d %d/%d/%d"
 */

enum LoadFlags {
    LoadFlags_Default,
    LoadFlags_NoTexcoords,
};

scene::Chunk load_obj_with_face_fmt(const char* path, LoadFlags flags, float scale);

/**
 * Takes a triangle soup (big_chunk) and returns an array of chunks with
 * at most "limit" triangles each. Each chunk's triangles are close together.
 */
Slice<scene::Chunk> shatter(scene::Chunk big_chunk, int limit);

}  // ns mesh
}  // ns ph
