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
    return load_obj_with_face_fmt(path, LoadFlags_Default, scale);
}

scene::Chunk load_obj_with_face_fmt(const char* path, LoadFlags flags, float scale) {
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
                    int a,b,c,
                        d,e,f,
                        g,h,k;
                    a = b = c = d = e = f = g = h = k = 0;
                    switch (flags) {
                    case LoadFlags_Default:
                        sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                                &a,&b,&c,
                                &d,&e,&f,
                                &g,&h,&k);
                        break;
                    case LoadFlags_NoTexcoords:
                        sscanf(line, "f %d//%d %d//%d %d//%d",
                                &a, &c,
                                &d, &f,
                                &g, &k);
                        break;
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

                    break;
                }
            default:
                {
                    break;
                }
            }
            if (count(faces) >= 50000) {
                break;
            }
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

Slice<scene::Chunk> shatter(scene::Chunk big_chunk, int limit, int depth) {
    ph_assert(big_chunk.num_verts % 3 == 0);
    // Base cases:
    auto size = big_chunk.num_verts;
    if (size == 0) {                                // size 0 => Return an empty slice.
        Slice<scene::Chunk> slice;
        slice.ptr = NULL;
        slice.n_elems = 0;
        slice.n_capacity = 0;
        return slice;
    }
    static int kMaxDepth = 1000;
    if (size < limit || depth >= kMaxDepth) {                             // size < limit => Return a slice of one.
        auto slice = MakeSlice<scene::Chunk>(1);
        scene::Chunk chunk;
        chunk.num_verts = big_chunk.num_verts;
        chunk.verts = phalloc(glm::vec3, big_chunk.num_verts);
        chunk.norms = phalloc(glm::vec3, big_chunk.num_verts);
        memcpy(chunk.verts, big_chunk.verts,
                sizeof(glm::vec3) * (size_t)big_chunk.num_verts);
        memcpy(chunk.norms, big_chunk.norms,
                sizeof(glm::vec3) * (size_t)big_chunk.num_verts);
        append(&slice, chunk);
        return slice;
    }

    // Recursion
    // 1) make a bounding box.
    scene::AABB bbox;
    bbox.xmin = big_chunk.verts[0].x;
    bbox.xmax = big_chunk.verts[0].x;
    bbox.ymin = big_chunk.verts[0].y;
    bbox.ymax = big_chunk.verts[0].y;
    bbox.zmin = big_chunk.verts[0].z;
    bbox.zmax = big_chunk.verts[0].z;
    for (int64 i = 0; i < big_chunk.num_verts; ++i) {
        auto vert = big_chunk.verts[i];
        if (vert.x < bbox.xmin) bbox.xmin = vert.x;
        if (vert.x > bbox.xmax) bbox.xmax = vert.x;
        if (vert.y < bbox.ymin) bbox.ymin = vert.y;
        if (vert.y > bbox.ymax) bbox.ymax = vert.y;
        if (vert.z < bbox.zmin) bbox.zmin = vert.z;
        if (vert.z > bbox.zmax) bbox.zmax = vert.z;
    }
    // 2) calculate centroid
    glm::vec3 centroid = scene::get_centroid(bbox);
    //printf("Centroid is: %s\n", str(centroid));
    // 3) Make 8 chunks. Fill each one with triangles corresponding to eight corners.
    scene::Chunk chunks[2][2][2];
    Slice<glm::vec3> vert_slices[2][2][2];
    Slice<glm::vec3> norm_slices[2][2][2];

    // Fill memory.
    for (int j = 0; j < 8; ++j) {
        ((scene::Chunk*)chunks)[j].num_verts = 0;
        ((Slice<glm::vec3>*)vert_slices)[j] = MakeSlice<glm::vec3>((size_t)2);
        ((Slice<glm::vec3>*)norm_slices)[j] = MakeSlice<glm::vec3>((size_t)2);
    }

    // Fill chunks.
    for (int j = 0; j < big_chunk.num_verts; j += 3) {
        // Front or back.
        int x,y,z;  // Three axis of octree. 0 or 1
        // get center of triangle
        glm::vec3 center;
        for (int k = 0; k < 3; ++k) {
            auto vert = big_chunk.verts[j + k];
            center.x += vert.x;
            center.y += vert.y;
            center.z += vert.z;
        }
        center.x /= 3.0f;
        center.y /= 3.0f;
        center.z /= 3.0f;
        x = int(center.x < centroid.x);
        y = int(center.y < centroid.y);
        z = int(center.z < centroid.z);

        // Add triangle
        for (int k = 0; k < 3; ++k) {
            auto vert = big_chunk.verts[j + k];
            auto norm = big_chunk.norms[j + k];
            chunks[z][y][x].num_verts++;
            append(&vert_slices[z][y][x], vert);
            append(&norm_slices[z][y][x], norm);

            //chunks[z][y][x].verts[num] = vert;
            //chunks[z][y][x].norms[num] = norm;
        }
    }
    for (int iz = 0; iz < 2; iz++) {
        for (int iy = 0; iy < 2; iy++) {
            for (int ix = 0; ix < 2; ix++) {
                chunks[iz][iy][ix].verts = vert_slices[iz][iy][ix].ptr;
                chunks[iz][iy][ix].norms = norm_slices[iz][iy][ix].ptr;
            }
        }
    }
    // 4) Recurse.
    // TODO: don't do it 8 times first, do it once per iteration to reduce stack overflow chances.
    auto slice = MakeSlice<scene::Chunk>(8);
    Slice<scene::Chunk> subchunks[8];
    for (int j = 0; j < 8; ++j) {
        if (((scene::Chunk*)chunks)[j].num_verts == 0) {
            static int c = 0;
            c++;
            subchunks[j].n_elems = 0;
            subchunks[j].ptr = NULL;
        } else {
            subchunks[j] = shatter(((scene::Chunk*)chunks)[j], limit, depth + 1);
        }
        phree(((scene::Chunk*)chunks)[j].verts);
        phree(((scene::Chunk*)chunks)[j].norms);
    }

    // 5) Append into one slice and return.
    for (int j = 0; j < 8; ++j) {
        auto subchunk = subchunks[j];
        for (int k = 0; k < count(subchunk); ++k) { // Clone chunks and append.
            scene::Chunk chunk;
            chunk.num_verts = subchunk[k].num_verts;
            chunk.verts = NULL;
            chunk.norms = NULL;
            if (chunk.num_verts) {
                chunk.verts = phalloc(glm::vec3, chunk.num_verts);
                chunk.norms = phalloc(glm::vec3, chunk.num_verts);
                memcpy(chunk.verts, subchunk[k].verts,
                        sizeof(glm::vec3) * (size_t)subchunk[k].num_verts);
                memcpy(chunk.norms, subchunk[k].norms,
                        sizeof(glm::vec3) * (size_t)subchunk[k].num_verts);
                phree(subchunk[k].verts);
                phree(subchunk[k].norms);
            }
            append(&slice, chunk);
        }
        release(&subchunk);
    }

    return slice;
}


}  // ns mesh
}  // ns ph
