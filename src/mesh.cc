#include "mesh.h"

namespace ph {
namespace mesh {

// Debugging
static const char* str(const glm::vec3& v) {
    char* out = phanaged(char, 16);
    sprintf(out, "%f, %f, %f", v.x, v.y, v.z);
    return out;
}

scene::Chunk load_obj(const char* path, float scale) {
    char* model_str_raw = (char *)io::slurp(path);

    typedef char* charptr;
    auto lines = MakeSlice<charptr>(1024);

    { // Read into lines
        const char* line = strtok(model_str_raw, "\n");
        while (line) {
            if (
                    0 == strncmp("v ", line, 2) ||
                    0 == strncmp("vn ", line, 3) ||
                    0 == strncmp("f ", line, 2)
               ) {
                append(&lines, (charptr)line);
            }
            line = strtok(NULL, "\n");
        }
    }

    struct Face {
        // Just indices into vertex/normal arrays.
        int64 vert_i[3];
        int64 norm_i[3];
    };

    // Raw model data.
    auto verts = MakeSlice<glm::vec3>(1024);
    auto norms = MakeSlice<glm::vec3>(1024);
    auto faces = MakeSlice<Face>(1024);

    {  // Parse obj format
        enum {
            Type_vert,
            Type_norm,
            Type_face,
            Type_count,
        };

        for (int64 i = 0; i < count(lines); ++i) {
            auto* line = lines[i];
            int type = Type_count;
            if (0 == strncmp("vn", line, 2)) {
                type = Type_norm;
            } else if (0 == strncmp("v", line, 1)) {
                type = Type_vert;
            } else if (0 == strncmp("f", line, 1)) {
                type = Type_face;
            }
            float x,y,z;
            switch(type) {
            case Type_vert:
                {
                    sscanf(line, "v %f %f %f", &x, &y, &z);
                    x *= scale;
                    y *= scale;
                    z *= scale;
                    append(&verts, glm::vec3(x, y, z));
                    /* printf("VERT: %f %f %f\n", x, y, z); */
                    break;
                }
            case Type_norm:
                {
                    sscanf(line, "vn %f %f %f", &x, &y, &z);
                    /* printf("NORM: %f %f %f\n", x, y, z); */
                    append(&norms, glm::vec3(x, y, z));
                    break;
                }
            case Type_face:
                {
                    enum {
                        DEFAULT,
                        NO_TEXCOORDS,
                        QUAD,
                    };
                    // Determine type of face.
                    int facetype = DEFAULT;
                    {
                        auto len = strlen(line);
                        int num_slashes = 0;
                        size_t prev = (size_t)-1;
                        for (size_t i = 0; i < len; ++i) {
                            char c = line[i];
                            if (c == '/') {
                                if (num_slashes == 1 && prev == (i - 1)) {
                                    facetype = NO_TEXCOORDS;
                                    break;
                                }
                                prev = i;
                                num_slashes++;
                            }
                        }
                        if (num_slashes == 8) {
                            facetype = QUAD;
                        }
                    }
                    int a,b,c,
                        d,e,f,
                        g,h,k,
                        l,m,n;
                    a = b = c = d = e = f = g = h = k = l = m = n = 0;
                    if (facetype == DEFAULT) {
                            sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                                    &a,&b,&c,
                                    &d,&e,&f,
                                    &g,&h,&k);
                    } else if (facetype == NO_TEXCOORDS) {
                        sscanf(line, "f %d//%d %d//%d %d//%d",
                                &a, &c,
                                &d, &f,
                                &g, &k);
                    } else if (facetype == QUAD) {
                        sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
                                    &a,&b,&c,
                                    &d,&e,&f,
                                    &g,&h,&k,
                                    &l,&m,&n);
                    }

                    /* printf("%d/%d %d/%d %d/%d\n", a,c, d,f, g,k); */
                    Face face;
                    face.vert_i[0] = a;
                    face.vert_i[1] = d;
                    face.vert_i[2] = g;

                    face.norm_i[0] = c;
                    face.norm_i[1] = f;
                    face.norm_i[2] = k;
                    append(&faces, face);

                    if (facetype == QUAD) {
                        face.vert_i[0] = a;
                        face.vert_i[1] = g;
                        face.vert_i[2] = l;

                        face.norm_i[0] = c;
                        face.norm_i[1] = k;
                        face.norm_i[2] = n;
                        append(&faces, face);
                    }
                    break;
                }
            default:
                {
                    break;
                }
            }
            //if (count(faces) > 100000) break;
        }
    }
    logf("num faces %ld\n", count(faces));
    scene::Chunk chunk;
    {  // Fill chunk.
        // Copies from vert / norms so that they form triangles.
        // Uses way more memory, but no need for Face structure.
        auto in_verts = MakeSlice<glm::vec3>(3 * (size_t)count(verts));
        auto in_norms = MakeSlice<glm::vec3>(3 * (size_t)count(norms));
        for (int64 i = 0; i < count(faces); ++i) {
            auto face = faces[i];
            for (int j = 0; j < 3; ++j) {
                auto vert = verts[face.vert_i[j] - 1];  // OBJ format indices are 1-based.
                auto norm = norms[face.norm_i[j] - 1];
                append(&in_verts, vert);
                append(&in_norms, norm);
            }
        }
        ph_assert(count(in_norms) == count(in_verts));
        chunk.verts = in_verts.ptr;
        chunk.norms = in_norms.ptr;
        chunk.num_verts = count(in_verts);
    }
    release(&verts);
    release(&norms);
    release(&faces);
    release(&lines);
    return chunk;
}

struct Bound {
    int64 a;
    int64 b;
};

static Bound make_bound(int64 a, int64 b) {
    Bound bound;
    bound.a = a;
    bound.b = b;
    return bound;
}

Slice<scene::Chunk> shatter(scene::Chunk big_chunk, int limit) {
    auto slice = MakeSlice<scene::Chunk>(1);  // The thing that we return

    auto size = big_chunk.num_verts;

    auto bounds = MakeSlice<Bound>(8); // Octree divisions, each iteration divides a bound into 8.
    append(&bounds, make_bound(0, size));
    while(count(bounds)) {
        auto bound = pop(&bounds);
        auto a = bound.a;
        auto b = bound.b;
        ph_assert(((b - a) % 3) == 0);
        // Find bbox and centroid.
        scene::AABB bbox;
        scene::bbox_fill(&bbox);
        for (auto i = a; i < b; i+=3) {
            auto a = big_chunk.verts[i + 0];
            auto b = big_chunk.verts[i + 1];
            auto c = big_chunk.verts[i + 2];
            auto vert = a + b + c;
            vert.x /= 3;
            vert.y /= 3;
            vert.z /= 3;
            if (vert.x < bbox.xmin) bbox.xmin = vert.x;
            if (vert.x > bbox.xmax) bbox.xmax = vert.x;
            if (vert.y < bbox.ymin) bbox.ymin = vert.y;
            if (vert.y > bbox.ymax) bbox.ymax = vert.y;
            if (vert.z < bbox.zmin) bbox.zmin = vert.z;
            if (vert.z > bbox.zmax) bbox.zmax = vert.z;
        }
        auto centroid = scene::get_centroid(bbox);
        Bound new_bounds[8];
        // Sort: for every vert, compare and move to the front
        int64 front = a;
        enum {
            TOP_LEFT_FRONT,
            TOP_LEFT_BACK,
            TOP_RIGHT_FRONT,
            TOP_RIGHT_BACK,
            BOT_LEFT_FRONT,
            BOT_LEFT_BACK,
            BOT_RIGHT_FRONT,
            BOT_RIGHT_BACK,
        };
        for (int q = 0; q < 8; q++) {
            new_bounds[q].a = front;
            // Top left front
            for (auto i = a; i < b; i += 3) {
                auto a = big_chunk.verts[i + 0];
                auto b = big_chunk.verts[i + 1];
                auto c = big_chunk.verts[i + 2];
                auto tri_centroid = a + b + c;
                tri_centroid.x /= 3.0f;
                tri_centroid.y /= 3.0f;
                tri_centroid.z /= 3.0f;
                bool do_swap = false;
                if (
                        (q == TOP_LEFT_FRONT &&
                         tri_centroid.x <= centroid.x &&
                         tri_centroid.y <= centroid.y &&
                         tri_centroid.z <= centroid.z
                        ) ||
                        (q == TOP_LEFT_BACK &&
                         tri_centroid.x <= centroid.x &&
                         tri_centroid.y <= centroid.y &&
                         tri_centroid.z > centroid.z
                        ) ||
                        (q == TOP_RIGHT_BACK &&
                         tri_centroid.x > centroid.x &&
                         tri_centroid.y <= centroid.y &&
                         tri_centroid.z > centroid.z
                        ) ||
                        (q == TOP_RIGHT_FRONT &&
                         tri_centroid.x > centroid.x &&
                         tri_centroid.y <= centroid.y &&
                         tri_centroid.z <= centroid.z
                        ) ||
                        (q == BOT_LEFT_BACK &&
                         tri_centroid.x <= centroid.x &&
                         tri_centroid.y > centroid.y &&
                         tri_centroid.z > centroid.z
                        ) ||
                        (q == BOT_LEFT_FRONT &&
                         tri_centroid.x <= centroid.x &&
                         tri_centroid.y > centroid.y &&
                         tri_centroid.z <= centroid.z
                        ) ||
                        (q == BOT_RIGHT_BACK &&
                         tri_centroid.x > centroid.x &&
                         tri_centroid.y > centroid.y &&
                         tri_centroid.z > centroid.z
                        ) ||
                        (q == BOT_RIGHT_FRONT &&
                         tri_centroid.x > centroid.x &&
                         tri_centroid.y > centroid.y &&
                         tri_centroid.z <= centroid.z
                        )
                        ) {
                            do_swap = true;
                        }
                if (do_swap) {
                    for (int j = 0; j < 3; ++j) {
                        auto tmp = big_chunk.verts[front + j];
                        big_chunk.verts[front + j] = big_chunk.verts[i + j];
                        big_chunk.verts[i + j] = tmp;

                        tmp = big_chunk.norms[front + j];
                        big_chunk.norms[front + j] = big_chunk.norms[i + j];
                        big_chunk.norms[i + j] = tmp;
                    }
                    front += 3;
                }
            }
            new_bounds[q].b = front;
        }
        for (int q = 0; q < 8; q++) {
            long bsize = (long)new_bounds[q].b - (long)new_bounds[q].a;
            if (bsize == 0) {
                // do nothing
            } else if (bsize < limit) {
                ph_assert (!(new_bounds[q].b == b && new_bounds[q].a == a));
                scene::Chunk chunk;
                chunk.verts = phalloc(glm::vec3, bsize);
                chunk.norms = phalloc(glm::vec3, bsize);
                for (long i = 0; i < bsize; ++i) {
                    chunk.verts[i] = big_chunk.verts[new_bounds[q].a + i];
                    chunk.norms[i] = big_chunk.norms[new_bounds[q].a + i];
                }
                chunk.num_verts = bsize;
                append(&slice, chunk);
            } else {
                append(&bounds, new_bounds[q]);
            }
        }
    }

    return slice;
}

}  // ns mesh
}  // ns ph
