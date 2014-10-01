
// 2014 Sergio Gonzalez

#include <ph.h>
#include <scene.h>
#include <vr.h>

#include "samples.h"

using namespace ph;

static void bunny_idle() {
    vr::draw(g_resolution);
}

static const char* str(const glm::vec3& v) {
    char* out = phanaged(char, 16);
    sprintf(out, "%f, %f, %f", v.x, v.y, v.z);
    return out;
}

struct Face {
    // Just indices into vertex/normal arrays.
    int64 vert_i[3];
    int64 norm_i[3];
};

/**
 * Represents triangle data. Small enough to be considered a primitive.
 * Large enough so that it justifies a bounding box.
 */
struct Chunk {
    glm::vec3* verts;
    glm::vec3* norms;
    Face* faces;
    int _padding;
    int64 num_verts;
    int64 num_norms;
    int64 face_count;
};

/*
 * Loading only vertices and normals.
 */
static Chunk load_obj(const char* path) {
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
            char* token = strtok(line, " ");
            int type = Type_count;
            if (0 == strncmp("v", line, 2)) {
                type = Type_vert;
            }
            if (0 == strncmp("vn", line, 2)) {
                type = Type_norm;
            }
            if (0 == strncmp("f", line, 2)) {
                type = Type_face;
            }
            float x,y,z;
            char* sx;
            char* sy;
            char* sz;
            Face face;
            face.vert_i[0] = -1;  // Garbage initialize. Should be filled when a face is found.

            if (type == Type_vert || type == Type_norm || type == Type_face) {
                sx = strtok(NULL, " ");
                sy = strtok(NULL, " ");
                sz = strtok(NULL, " ");
                ph_assert(sx);
                ph_assert(sy);
                ph_assert(sz);
                char* end;
                if (type == Type_vert || type == Type_norm) {
                    x = strtof(sx, &end);
                    y = strtof(sy, &end);
                    z = strtof(sz, &end);
                    if (x != x || y != y || z != z) {
                        fprintf(stderr, "NaN error");
                        printf("%f %f %f\n", x, y, z);
                    }
                    token = strtok(NULL, " ");  // Make null
                } else if (type == Type_face) {
                    char* strings[3] = {
                        sx, sy, sz
                    };

                    for (int j = 0; j < 3; ++j) {
                        char* s = strings[j];
                        char* first  = strchr(s, '/');
                        char* second = strrchr(s, '/');
                        *first = '\0';  // destroys the string...
                        second++;
                        /* printf("FACE: v:%s, n:%s ==== \n", s, second); */
                        face.vert_i[j] = atoi(s);
                        face.norm_i[j] = atoi(second);
                    }
                    token = strtok(NULL, " ");  // Make null
                }
                // Make null
                while(token) {
                    token = strtok(NULL, " ");
                }
                if (type == Type_vert) {
                    append(&verts, glm::vec3(x, y, z));
                    /* printf("appended vert %s\n", str(verts[count(verts) - 1])); */
                } else if (type == Type_norm) {
                    append(&norms, glm::vec3(x, y, z));
                    /* printf("appended norm %s\n", str(norms[count(norms) - 1])); */
                } else if (type == Type_face) {
                    append(&faces, face);
                }
                ph_assert(!token);
            }
        }
    }
    Chunk chunk;
    chunk.verts = verts.ptr;
    chunk.norms = norms.ptr;
    chunk.num_verts = count(verts);
    chunk.num_norms = count(norms);
    chunk.faces = faces.ptr;
    chunk.face_count = count(faces);
    return chunk;
}

void bunny_sample() {
    scene::init();

    load_obj("third_party/bunny.obj");

    window::main_loop(bunny_idle);
}
