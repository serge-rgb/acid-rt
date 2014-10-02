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

/*
 * Loading only vertices and normals.
 */
static scene::Chunk load_obj(const char* path, float scale) {
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
                    /* int a,b,c, */
                    /*     d,e,f, */
                    /*     g,h,k; */
                    /* sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d", */
                    /*         &a,&b,&c, */
                    /*         &d,&e,&f, */
                    /*         &g,&h,&k); */
                    int a,c,
                        d,f,
                        g,k;
                    sscanf(line, "f %d//%d %d//%d %d//%d",
                            &a, &c,
                            &d, &f,
                            &g, &k);
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
        }
    }
    scene::Chunk chunk;
    {  // Fill chunk.
        // Copies from vert / norms so that they form triangles.
        // Uses way more memory, but no need for Face structure.
        static int limit = 5000;
        int n = 0;
        auto in_verts = MakeSlice<glm::vec3>(3 * (size_t)count(verts));
        for (int64 i = 0; i < count(faces); ++i) {
            auto face = faces[i];
            for (int j = 0; j < 3; ++j) {
                auto vert = verts[face.vert_i[j] - 1];  // OBJ format indices are 1-based.
                append(&in_verts, vert);
                n++;
            }
            if (n > limit) break;
        }
        chunk.verts = in_verts.ptr;
        chunk.num_verts = count(in_verts);
    }

    return chunk;
}

void bunny_sample() {
    scene::init();

    auto chunk = load_obj("third_party/bunny.obj", 10);
    scene::submit_primitive(&chunk);
    scene::update_structure();
    scene::upload_everything();

    window::main_loop(bunny_idle);
}
