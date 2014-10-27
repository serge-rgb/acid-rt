#pragma once

#include <ph.h>

////////////////////////////////////////
//          -- ph::scene --
// Handle the creation of primitives
//  (triangle meshes, essentially)
// Handle the description of primitives
//  (i.e. material data)
// Handle submitting primitives to the
// GPU program.
//  (i.e. Acceleration structures)
////////////////////////////////////////
namespace ph {
namespace scene {

enum SubmitFlags {
    SubmitFlags_None        = 1 << 0,
    SubmitFlags_FlipNormals = 1 << 1,
    SubmitFlags_Update      = 1 << 2,
};

struct AABB {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
    float zmin;
    float zmax;
};

glm::vec3 get_centroid(AABB b);

struct Cube {
    glm::vec3 center;
    glm::vec3 sizes;
    int32 index = -1; //  Place in triangle pool where associated triangles begin.
};

Cube make_cube(float x, float y, float z, float size);
Cube make_cube(float x, float y, float z, float size_x, float size_y, float size_z);

struct Rect {
    float x;
    float y;
    float w;
    float h;
};

Rect cube_to_rect(scene::Cube cube);

bool collision_p(Rect a, Rect b);

/**
 * Represents triangle data. Small enough to be considered a primitive.
 * Large enough so that it justifies a bounding box.
 */
struct Chunk {
    glm::vec3* verts;
    glm::vec3* norms;  // One for each vertex
    int64 num_verts;
};

void init();

// ---- submit_primitive
// Add primitives to scene.

int64 submit_primitive(Cube* cube, SubmitFlags flags = SubmitFlags_None, int64 flag_params = 0);
int64 submit_primitive(Chunk* chunk, SubmitFlags flags = SubmitFlags_None, int64 flag_params = 0);

// ----------------------

// ---- After submitting or updating, update the acceleration structure

// Build acceleration structure,
// then upload it to GPU.
void update_structure();

// ----------------------

// ---- Functions to upload scene info to GPU.

// Submit data about primitives to GPU.
void upload_everything();

// ----------------------

}  // ns scene
}  // ns ph
