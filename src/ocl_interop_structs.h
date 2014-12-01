
#include "AABB.h"

#pragma once

namespace ph
{

// Plain and simple struct for flattened tree.
struct BVHNode
{
    int primitive_offset;       // >0 when leaf. -1 when not.
    int right_child_offset;     // Left child is adjacent to node. (-1 if leaf!)
    AABB bbox;
    // 4 + 4 + (6 * 4 = 24) = 8 + 24 = 32 = (16 * 2) ... So it's 16 byte aligned
};

struct CLvec3
{
    float x;
    float y;
    float z;
    float _padding;
};

struct CLtriangle
{
    CLvec3 p0;
    CLvec3 p1;
    CLvec3 p2;
    // 4 * 3 = 12. Add padding.
    CLvec3 _padding;
};

// Note: When brute force ray tracing:
// The extra level of indirection has no perceivable overhead compared to
// tracing against a triangle pool.
struct Primitive
{
    int offset;             // Num of elements into the triangle pool where this primitive begins.
    int num_triangles;
    int material;           // Enum (copy in shader).
};
}
