#include "scene.h"

namespace ph {
namespace scene {

// For DFS buffers.
static const int kTreeStackLimit = 30;

// Defined below
struct GLtriangle;
struct GLlight;
struct Primitive;

static Slice<GLtriangle>    m_triangle_pool;
static Slice<GLtriangle>    m_normal_pool;
static Slice<GLlight>       m_light_pool;
static Slice<Primitive>     m_primitives;
static int                  m_debug_bvh_height = -1;
static GLuint               m_bvh_buffer;
static GLuint               m_triangle_buffer;
static GLuint               m_normal_buffer;
static GLuint               m_light_buffer;
static GLuint               m_prim_buffer;


struct GLvec3 {
    float x;
    float y;
    float z;
    float _padding;
};

struct GLtriangle {
    GLvec3 p0;
    GLvec3 p1;
    GLvec3 p2;
};

struct GLlight {
    GLvec3 position;
    GLvec3 color;
};

struct Light {
    GLlight data;
    int64 index;
};

enum MaterialType {
    MaterialType_Lambert,
};

// Note: When brute force ray tracing:
// The extra level of indirection has no perceivable overhead compared to
// tracing against a triangle pool.
struct Primitive {
    int offset;             // Num of elements into the triangle pool where this primitive begins.
    int num_triangles;
    int material;           // Enum (copy in shader).
};

static const char* str(const glm::vec3& v) {
    char* out = phanaged(char, 16);
    sprintf(out, "%f, %f, %f", v.x, v.y, v.z);
    return out;
}

static const char* str(AABB b) {
    char* out = phanaged(char, 16);
    sprintf(out, "%f, %f\n%f, %f\n%f, %f\n", b.xmin, b.xmax, b.ymin, b.ymax, b.zmin, b.zmax);
    return out;
}

static AABB get_bbox(const Primitive* primitives, int count) {
    ph_assert(count > 0);
    AABB bbox;
    { // Fill bbox with not-nonsense
        auto first = m_triangle_pool[primitives[0].offset];
        bbox.xmin = first.p0.x;
        bbox.xmax = first.p0.x;
        bbox.ymin = first.p0.y;
        bbox.ymax = first.p0.y;
        bbox.zmin = first.p0.z;
        bbox.zmax = first.p0.z;
    }
    for (int pi = 0; pi < count; ++pi) {
        auto primitive = primitives[pi];
        if (primitive.offset < 0) {
            return bbox;
        }

        for (int i = primitive.offset; i < primitive.offset + primitive.num_triangles; ++i) {
            GLtriangle tri = m_triangle_pool[i];
            GLvec3 points[3] = { tri.p0, tri.p1, tri.p2 };
            for (int j = 0; j < 3; ++j) {
                auto p = points[j];
                if (p.x < bbox.xmin) bbox.xmin = p.x;
                if (p.x > bbox.xmax) bbox.xmax = p.x;
                if (p.y < bbox.ymin) bbox.ymin = p.y;
                if (p.y > bbox.ymax) bbox.ymax = p.y;
                if (p.z < bbox.zmin) bbox.zmin = p.z;
                if (p.z > bbox.zmax) bbox.zmax = p.z;
            }
        }
    }
    return bbox;
}

static AABB bbox_union(AABB a, AABB b) {
    AABB res;
    res.xmin = fmin(a.xmin, b.xmin);
    res.ymin = fmin(a.ymin, b.ymin);
    res.zmin = fmin(a.zmin, b.zmin);
    res.xmax = fmax(a.xmax, b.xmax);
    res.ymax = fmax(a.ymax, b.ymax);
    res.zmax = fmax(a.ymax, b.zmax);
    return res;
}

static float bbox_area(AABB bbox) {
    // Test for just-initialized bbox.
    if (bbox.xmin == INFINITY) return 0;
    glm::vec3 d;
    d.x = bbox.xmax - bbox.xmin;
    d.y = bbox.ymax - bbox.ymin;
    d.z = bbox.zmax - bbox.zmin;
    return 2.0f * (d.x * d.x + d.y * d.y + d.z * d.z);
}

static void bbox_fill(AABB* bbox) {
    bbox->xmax = -INFINITY;
    bbox->ymax = -INFINITY;
    bbox->zmax = -INFINITY;
    bbox->xmin = INFINITY;
    bbox->ymin = INFINITY;
    bbox->zmin = INFINITY;
}

////////////////////////////////////////
// BVH Accel
////////////////////////////////////////

// Plain and simple struct for flattened tree.
struct BVHNode {
    int primitive_offset;       // >0 when leaf. -1 when not.
    int right_child_offset;     // Left child is adjacent to node. (-1 if leaf!)
    AABB bbox;
};

// Big fat struct for tree construction
struct BVHTreeNode {
    BVHNode data;  // Fill this and write it to the array
    // Children
    BVHTreeNode* left;
    BVHTreeNode* right;
};

enum SplitPlane {
    SplitPlane_X,
    SplitPlane_Y,
    SplitPlane_Z,
};

// Returns a memory managed BVH tree from primitives.
// 'indices' keeps the original order of the slice.
// bbox_cache avoids recomputation of bboxes for single primitives.
static BVHTreeNode* build_bvh(
        Slice<Primitive> primitives, const int32* indices, AABB* bbox_cache, int axis_split) {
    BVHTreeNode* node = phalloc(BVHTreeNode, 1);
    BVHNode data;
    data.primitive_offset = -1;
    data.right_child_offset = -1;

    ph_assert(count(primitives) != 0);
    data.bbox = get_bbox(primitives.ptr, (int)count(primitives));

    if (count(primitives) == 1) {           // ---- Leaf
        data.primitive_offset = *indices;
        node->left = NULL;
        node->right = NULL;
    } else {                                // ---- Inner node
        auto centroids = MakeSlice<glm::vec3>((size_t)count(primitives));
        glm::vec3 midpoint;

        { // Fill centroids. Calculate midpoint.
            for (int i = 0; i < count(primitives); ++i) {
                auto ci = append(&centroids, get_centroid(get_bbox(&primitives[i], 1)));
                midpoint.x += centroids[ci].x;
                midpoint.y += centroids[ci].y;
                midpoint.z += centroids[ci].z;
            }
            ph_assert(count(centroids) == count(primitives));
            midpoint /= count(primitives);
        }

        // Fill variations.

        float variation[3] = { 0, 0, 0 };  // Used to choose split axis
        {
            float minx, maxx, miny, maxy, minz, maxz;
            minx = maxx = centroids[0].x;
            miny = maxy = centroids[0].y;
            minz = maxz = centroids[0].z;
            for (int i = 1; i < count(centroids); ++i) {
                if (centroids[i].x < minx) minx = centroids[i].x;
                if (centroids[i].x > maxx) maxx = centroids[i].x;
                if (centroids[i].y < miny) miny = centroids[i].y;
                if (centroids[i].y > maxy) maxy = centroids[i].y;
                if (centroids[i].z < minz) minz = centroids[i].z;
                if (centroids[i].z > maxz) maxz = centroids[i].z;
            }
            variation[0] = fabs(maxx - minx);
            variation[1] = fabs(maxy - miny);
            variation[2] = fabs(maxz - minz);
        }

        int split = SplitPlane_X;
        split = axis_split;

        { // Choose split plane
            float v = -1;
            for (int i = 0; i < 3; i++) {
                if (v < variation[i]) {
                    v = variation[i];
                    split = SplitPlane(i);
                }
            }
            if (v == 0.0f) {
                log("Two objets have the same centroid. Using default axis split");
                split = axis_split;
            }
        }

        // Make two new slices.
        auto slice_left = MakeSlice<Primitive>(size_t(count(primitives) / 2));
        auto slice_right = MakeSlice<Primitive>(size_t(count(primitives) / 2));
        // These indices keep the old ordering.
        int* new_indices_l = phalloc(int, (size_t)count(primitives));
        int* new_indices_r = phalloc(int, (size_t)count(primitives));
        int offset_l = 0;
        int offset_r = 0;
        memcpy(new_indices_l, indices, sizeof(int) * (size_t)count(primitives));
        memcpy(new_indices_r, indices, sizeof(int) * (size_t)count(primitives));

        if (count(primitives) == 2) {
            append(&slice_left, primitives[0]);
            append(&slice_right, primitives[1]);
            new_indices_l[offset_l++] = indices[0];
            new_indices_r[offset_r++] = indices[1];
            node->left = build_bvh(slice_left, new_indices_l, bbox_cache, (split + 1) % 3);
            node->right = build_bvh(slice_right, new_indices_r, bbox_cache, (split + 1) % 3);
            release(&slice_left);
            release(&slice_right);
            node->data = data;
            return node;
        }


        bool use_sah = count(primitives) > 4;
        // ============================================================
        // SAH
        // ============================================================
        if (use_sah) {
            struct Bucket {
                int num;
                AABB bbox;
            };

            const int kNumBuckets = 8;
            Bucket buckets[kNumBuckets];

            auto bbox = data.bbox;  // Bounding box for primitives.

            // bbox as two vectors:
            glm::vec3 bbox_vmin;
            glm::vec3 bbox_vmax;
            bbox_vmin.x = bbox.xmin;
            bbox_vmin.y = bbox.ymin;
            bbox_vmin.z = bbox.zmin;
            bbox_vmax.x = bbox.xmax;
            bbox_vmax.y = bbox.ymax;
            bbox_vmax.z = bbox.zmax;


            // 1) Initialize info (bounds & count) for each bucket
            for (int i = 0; i < kNumBuckets; ++i) {
                bbox_fill(&buckets[i].bbox);
                buckets[i].num = 0;
            }
            auto bbox_centroid = get_centroid(bbox);

            int* assign = phalloc(int, (size_t)count(primitives));
            // ^--- Store which primitive is assigned to which bucket.

            for (int i = 0; i < count(primitives); ++i) {
                glm::vec3 centroid = centroids[i];
                int b = (int)(kNumBuckets *
                        (centroid[split] - bbox_vmin[split]) /
                        (bbox_vmax[split] - bbox_vmin[split]));
                if (b == kNumBuckets) { b--; }
                /* buckets[b].bbox = bbox_union(buckets[b].bbox, get_bbox(&primitives[i], 1)); */
                buckets[b].bbox = bbox_union(buckets[b].bbox, bbox_cache[i]);
                buckets[b].num++;
                assign[i] = b;
            }
            // 2) compute cost for each permutation: Sum of (bucketBbox / primitivesBbox)
            float cost[kNumBuckets];
            for (int i = 0; i < kNumBuckets; ++i) {
                AABB b0, b1;
                int count0 = 0;
                int count1 = 0;

                bbox_fill(&b0);
                bbox_fill(&b1);

                for (int j = 0; j <= i; ++j) {
                    b0 = bbox_union(b0, buckets[j].bbox);
                    count0 += buckets[j].num;
                }
                for (int j = i + 1; j < kNumBuckets; ++j) {
                    b1 = bbox_union(b1, buckets[j].bbox);
                    count1 += buckets[j].num;
                }
                //printf("counts: %d %d\n", count0, count1);
                //printf("areas: %f %f %f\n", bbox_area(b0), bbox_area(b1), bbox_area(bbox));
                cost[i] = 0.125f +
                    (count0 * bbox_area(b0) + count1 * bbox_area(b1)) /
                    (bbox_area(bbox));
                //logf("cost %d is %f\n", i , cost[i]);
            }
            // 3) find minimal cost split
            int min_split = 0;
            float min_cost = cost[0];
            for (int i = 1; i < kNumBuckets; ++i) {
                if (cost[i] < min_cost) {
                    min_cost = cost[i];
                    min_split = i;
                }
            }
            // 4) split
            // Use the assign array to send them left or right
            bool will_split = false;
            int c_left = 0;
            int c_right = 0;
            for (int i = 0; i < count(primitives); ++i) {
                if (assign[i] <= min_split) {
                    c_left++;
                }
                else {
                    c_right++;
                }
            }
            if (c_left != 0 && c_right != 0) {
                will_split = true;
            }

            static int noteven_count = 0;
            if (will_split) {
                noteven_count++;
                for (int i = 0; i < count(primitives); ++i) {
                    if (assign[i] <= min_split) {
                        /* log("left"); */
                        append(&slice_left, primitives[i]);
                        new_indices_l[offset_l++] = indices[i];
                    }
                    else {
                        /* log("right"); */
                        append(&slice_right, primitives[i]);
                        new_indices_r[offset_r++] = indices[i];
                    }
                }
            }
            else {
                static int even_count = 0;
                even_count++;
                logf("========= spliting evenly ========= %d vs %d\n", even_count, noteven_count);
                for (int i = 0; i < count(primitives); ++i) {
                    auto centroid = centroids[i];
                    if (centroid[split] < midpoint[split]) {
                        append(&slice_left, primitives[i]);
                        new_indices_l[offset_l++] = indices[i];
                    }
                    else {  // Default: to the right
                        append(&slice_right, primitives[i]);
                        new_indices_r[offset_r++] = indices[i];
                    }
                }
            }
            phree(assign);
        // ============================================================
        // Equal partition.
        // ============================================================
        } else {
            // Partition two slices based on which side of the midpoint.
            for (int i = 0; i < count(primitives); ++i) {
                auto centroid = centroids[i];
                if (centroid[split] < midpoint[split]) {
                    append(&slice_left, primitives[i]);
                    new_indices_l[offset_l++] = indices[i];
                }
                else {  // Default: to the right
                    append(&slice_right, primitives[i]);
                    new_indices_r[offset_r++] = indices[i];
                }
            }

        }
        ph_assert(!(offset_r != 0 && offset_l == 0) && !(offset_l != 0 && offset_r == 0));

        node->left = node->right = NULL;
        if (count(slice_left)) {
            node->left = build_bvh(slice_left, new_indices_l, bbox_cache, (split + 1) % 3);
        }
        if (count(slice_right)) {
            node->right = build_bvh(slice_right, new_indices_r, bbox_cache, (split + 1) % 3);
        }
        release(&slice_left);
        release(&slice_right);
        phree(new_indices_l);
        phree(new_indices_r);
        release(&centroids);
    }
    node->data = data;
    return node;
}

static bool validate_bvh(BVHTreeNode* root, Slice<Primitive> data) {
    BVHTreeNode* stack[kTreeStackLimit];
    int stack_offset = 0;
    bool* checks = phalloc(bool, count(data));  // Every element must be present once.
    stack[stack_offset++] = root;
    int height = 0;

    for (int i = 0; i < count(data); ++i) {
        checks[i] = false;
    }

    while (stack_offset > 0) {
        BVHTreeNode* node = stack[--stack_offset];
        if (node->left == NULL && node->right == NULL) {
            int i = node->data.primitive_offset;
            if (checks[i]) {
                printf("Found double leaf %d\n", i);
                return false;
            } else {
                checks[i] = true;
                /* printf("Leaf bounding box: \n%s", str(node->data.bbox)); */
                /* printf("Leaf prim: %d\n\n", node->data.primitive_offset); */
            }
            auto bbox = node->data.bbox;
            auto bbox0 = get_bbox(&data[i], 1);
            float epsilon = 0.00001f;
            bool check =
                bbox.xmin - bbox0.xmin < epsilon &&
                bbox.xmax - bbox0.xmax < epsilon &&
                bbox.ymin - bbox0.ymin < epsilon &&
                bbox.ymax - bbox0.ymax < epsilon &&
                bbox.zmin - bbox0.zmin < epsilon &&
                bbox.zmax - bbox0.zmax < epsilon;
            if (!check) {
                printf("Incorrect bounding box for leaf %d\n", i);
                return false;
            }
        } else {
            if (node->data.primitive_offset != -1) {
                printf("Non-leaf node has primitive!\n");
                return false;
            }
            if (node->left == NULL || node->right == NULL) {
                printf("Found null child on non-leaf node\n");
                return false;
            }
            ph_assert(stack_offset + 2 < kTreeStackLimit);
            stack[stack_offset++] = node->right;
            stack[stack_offset++] = node->left;
            if (stack_offset > height) {
                height = stack_offset;
            }
        }
    }

    for (int i = 0; i < count(data); ++i) {
        if (checks[i] == false) {
            printf("Leaf %d not found.\n", i);
            return false;
        }
    }

    phree(checks);
    /* printf("BVH valid.\n"); */
    return true;
}

void release(BVHTreeNode* root) {
    BVHTreeNode* stack[kTreeStackLimit];
    int stack_offset = 0;
    stack[stack_offset++] = root;
    while(stack_offset > 0) {
        BVHTreeNode* node = stack[--stack_offset];
        bool is_leaf = node->left == NULL && node->right == NULL;
        if (!is_leaf) {
            stack[stack_offset++] = node->left;
            stack[stack_offset++] = node->right;
        }
        phree(node);
    }
}

uint64_t hash(BVHTreeNode* data) {
    uint64_t hash = 5381;
    uint64_t hash_data[] = {
        (uint64_t)(data->data.primitive_offset),
        (uint64_t)(data->data.right_child_offset),
        (uint64_t)floorf(1000 * data->data.bbox.xmin),
        (uint64_t)floorf(1000 * data->data.bbox.xmax),
        (uint64_t)floorf(1000 * data->data.bbox.ymin),
        (uint64_t)floorf(1000 * data->data.bbox.ymax),
        (uint64_t)floorf(1000 * data->data.bbox.zmin),
        (uint64_t)floorf(1000 * data->data.bbox.zmax),
    };
    size_t limit = sizeof(hash_data) / sizeof(uint64_t);
    for (size_t sz = 0; sz < limit; sz++) {
        uint64_t v = hash_data[sz];
        hash = (hash * 33) ^ v;
    }
    return hash;
}

// Returns a memory-managed array of BVHNode in depth first order. Ready for GPU consumption.
static BVHNode* flatten_bvh(BVHTreeNode* root, int64* out_len) {
    auto slice = MakeSlice<BVHNode>(16);
    auto dict  = MakeDict < BVHTreeNode*, int64> (1000);

    BVHTreeNode* stack[kTreeStackLimit];
    int stack_offset = 0;
    stack[stack_offset++] = root;

    while(stack_offset > 0) {
        auto fatnode = stack[--stack_offset];
        BVHNode node = fatnode->data;
        int64 index = append(&slice, node);
        insert(&dict, fatnode, index);
        bool is_leaf = fatnode->left == NULL && fatnode->right == NULL;
        if (!is_leaf) {
            stack[stack_offset++] = fatnode->right;
            stack[stack_offset++] = fatnode->left;
        }
    }

    ////////////////
    // second pass
    ////////////////
    stack_offset = 0;
    stack[stack_offset++] = root;
    int i = 0;
    while(stack_offset > 0) {
        auto fatnode = stack[--stack_offset];
        bool is_leaf = fatnode->left == NULL && fatnode->right == NULL;
        if (!is_leaf) {
            stack[stack_offset++] = fatnode->right;
            stack[stack_offset++] = fatnode->left;
            int64 found_i = *find(&dict, fatnode->right);
            ph_assert(found_i >= 0);
            ph_assert(found_i < int64(1) << 31);
            slice.ptr[i].right_child_offset = (int)found_i;
        }
        i++;
    }

    *out_len = count(slice);

    release(&dict);
    return slice.ptr;
}

static bool validate_flattened_bvh(BVHNode* node, int64 len) {
    bool* check = phalloc(bool, len);
    for (int64 i = 0; i < len; ++i) {
        check[i] = false;
    }

    int64 num_leafs = 0;

    for (int64 i = 0; i < len; ++i) {
        /* printf("Node %ld: At its right: %d\n", i, node->right_child_offset); */
        if (node->primitive_offset != -1) {
            num_leafs++;
            /* printf("  Leaf! %d\n", node->primitive_offset); */
            if (check[node->primitive_offset]) {
                printf("Double leaf %d\n", node->primitive_offset);
                return false;
            }
            check[node->primitive_offset] = true;
        }
        node++;
    }

    for (int64 i = 0; i < num_leafs; ++i) {
        if (!check[i]) {
            printf("Missing leaf %ld\n", i);
            return false;
        }
    }
    phree(check);
    /* printf("Flat tree valid.\n"); */
    return true;
}

// Return a vec3 with layout expected by the compute shader.
// Reverse z while we're at it, so it is in view coords.
static GLvec3 to_gl(glm::vec3 in) {
    GLvec3 out = {in.x, in.y, in.z, 0};
    return out;
}

glm::vec3 get_centroid(AABB b) {
    return glm::vec3((b.xmax + b.xmin) / 2, (b.ymax + b.ymin) / 2, (b.zmax + b.zmin) / 2);
}

bool collision_p(Rect a, Rect b) {
    bool not_collides =
        ((b.x > a.x + a.w) || (b.x + b.w < a.x)) ||
        ((b.y > a.y + a.h) || (b.y + b.h < a.y));
    return !not_collides;
}

Rect cube_to_rect(scene::Cube cube) {
    scene::Rect rect;
    rect.x = cube.center.x - cube.sizes.x;
    rect.y = cube.center.y - cube.sizes.y;
    rect.w = cube.sizes.x * 2;
    rect.h = cube.sizes.y * 2;
    return rect;
}

int64 submit_light(Light* light) {
    light->index = append(&m_light_pool, light->data);
    return light->index;
}

int64 submit_primitive(Cube* cube, SubmitFlags flags, int64 flag_params) {
    // 6 points of cube
    //       d----c
    //      / |  /|
    //     a----b |
    //     |  g-|-f
    //     | /  |/
    //     h----e
    // I am an artist!

    // Vertex data
    glm::vec3 _a,_b,_c,_d,_e,_f,_g,_h;
    GLvec3 a,b,c,d,e,f,g,h;

    // Normal data for each face
    GLvec3 nf, nr, nb, nl, nt, nm;


    // Temp struct, fill-and-submit.
    GLtriangle tri;
    GLtriangle norm;

    // Return index to the first appended triangle.
    int64 index = -1;
    if (flags & SubmitFlags_Update) {
        index = cube->index;
    } else {  // Append 12 new triangles
        tri.p0.x = 0;  // Initialize garbage, just to get an index. Will be filled below.
        index = append(&m_triangle_pool, tri);
#ifdef PH_DEBUG
        auto n_index = append(&m_normal_pool, tri);
        ph_assert(n_index == index);
#else
        append(&m_normal_pool, tri);
#endif
        for (int i = 0; i < 11; ++i) {
            append(&m_triangle_pool, tri);
            append(&m_normal_pool, tri);
        }
    }

    _a = glm::vec3(cube->center + glm::vec3(-cube->sizes.x, cube->sizes.y, cube->sizes.z));
    _b = glm::vec3(cube->center + glm::vec3(cube->sizes.x, cube->sizes.y, cube->sizes.z));
    _c = glm::vec3(cube->center + glm::vec3(cube->sizes.x, cube->sizes.y, -cube->sizes.z));
    _d = glm::vec3(cube->center + glm::vec3(-cube->sizes.x, cube->sizes.y, -cube->sizes.z));
    _e = glm::vec3(cube->center + glm::vec3(cube->sizes.x, -cube->sizes.y, cube->sizes.z));
    _f = glm::vec3(cube->center + glm::vec3(cube->sizes.x, -cube->sizes.y, -cube->sizes.z));
    _g = glm::vec3(cube->center + glm::vec3(-cube->sizes.x, -cube->sizes.y, -cube->sizes.z));
    _h = glm::vec3(cube->center + glm::vec3(-cube->sizes.x, -cube->sizes.y, cube->sizes.z));


    a = to_gl(_a);
    b = to_gl(_b);
    c = to_gl(_c);
    d = to_gl(_d);
    e = to_gl(_e);
    f = to_gl(_f);
    g = to_gl(_g);
    h = to_gl(_h);


    // 6 normals
    nf = to_gl(glm::normalize(glm::cross(_b - _e, _h - _e)));
    nr = to_gl(glm::normalize(glm::cross(_c - _f, _e - _f)));
    nb = to_gl(glm::normalize(glm::cross(_d - _g, _f - _g)));
    nl = to_gl(glm::normalize(glm::cross(_a - _h, _g - _h)));
    nt = to_gl(glm::normalize(glm::cross(_c - _b, _a - _b)));
    nm = to_gl(glm::normalize(glm::cross(_e - _f, _g - _f)));

    if (flags & SubmitFlags_FlipNormals) {
        GLvec3* normals[] = {&nf, &nr, &nb, &nl, &nt, &nm};
        for (int i = 0; i < 6; ++i) {
            normals[i]->x *= -1;
            normals[i]->y *= -1;
            normals[i]->z *= -1;
        }
    }

    // Front face
    tri.p0 = h;
    tri.p1 = b;
    tri.p2 = a;
    norm.p0 = nf;
    norm.p1 = nf;
    norm.p2 = nf;
    m_triangle_pool[index + 0] = tri;
    m_normal_pool[index + 0] = norm;
    tri.p0 = h;
    tri.p1 = e;
    tri.p2 = b;
    norm.p0 = nf;
    norm.p1 = nf;
    norm.p2 = nf;
    m_triangle_pool[index + 1] = tri;
    m_normal_pool[index + 1] = norm;

    // Right
    tri.p0 = e;
    tri.p1 = c;
    tri.p2 = b;
    norm.p0 = nr;
    norm.p1 = nr;
    norm.p2 = nr;
    m_triangle_pool[index + 2] = tri;
    m_normal_pool[index + 2] = norm;
    tri.p0 = e;
    tri.p1 = c;
    tri.p2 = f;
    norm.p0 = nr;
    norm.p1 = nr;
    norm.p2 = nr;
    m_triangle_pool[index + 3] = tri;
    m_normal_pool[index + 3] = norm;

    // Back
    tri.p0 = d;
    tri.p1 = c;
    tri.p2 = g;
    norm.p0 = nb;
    norm.p1 = nb;
    norm.p2 = nb;
    m_triangle_pool[index + 4] = tri;
    m_normal_pool[index + 4] = norm;
    tri.p0 = c;
    tri.p1 = f;
    tri.p2 = g;
    norm.p0 = nb;
    norm.p1 = nb;
    norm.p2 = nb;
    m_triangle_pool[index + 5] = tri;
    m_normal_pool[index + 5] = norm;

    // Left
    tri.p0 = a;
    tri.p1 = h;
    tri.p2 = d;
    norm.p0 = nl;
    norm.p1 = nl;
    norm.p2 = nl;
    m_triangle_pool[index + 6] = tri;
    m_normal_pool[index + 6] = norm;
    tri.p0 = h;
    tri.p1 = d;
    tri.p2 = g;
    norm.p0 = nl;
    norm.p1 = nl;
    norm.p2 = nl;
    m_triangle_pool[index + 7] = tri;
    m_normal_pool[index + 7] = norm;
    tri.p0 = h;

    // Top
    tri.p0 = a;
    tri.p1 = c;
    tri.p2 = d;
    norm.p0 = nt;
    norm.p1 = nt;
    norm.p2 = nt;
    m_triangle_pool[index + 8] = tri;
    m_normal_pool[index + 8] = norm;
    tri.p0 = h;
    tri.p0 = a;
    tri.p1 = b;
    tri.p2 = c;
    norm.p0 = nt;
    norm.p1 = nt;
    norm.p2 = nt;
    m_triangle_pool[index + 9] = tri;
    m_normal_pool[index + 9] = norm;

    // Bottom
    tri.p0 = h;
    tri.p1 = f;
    tri.p2 = g;
    norm.p0 = nm;
    norm.p1 = nm;
    norm.p2 = nm;
    m_triangle_pool[index + 10] = tri;
    m_normal_pool[index + 10] = norm;
    tri.p0 = h;
    tri.p1 = e;
    tri.p2 = f;
    norm.p0 = nm;
    norm.p1 = nm;
    norm.p2 = nm;
    m_triangle_pool[index + 11] = tri;
    m_normal_pool[index + 11] = norm;

    ph_assert(index <= PH_MAX_int32);
    cube->index = (int)index;
    Primitive prim;
    prim.offset = cube->index;
    prim.num_triangles = 12;
    prim.material = MaterialType_Lambert;
    if (flags & SubmitFlags_Update) {
        ph_assert(flag_params >= 0 && flag_params < count(m_primitives));
        m_primitives[flag_params] = prim;
        return flag_params;
    } else {
        return append(&m_primitives, prim);
    }
}

static int64 submit_primitive(AABB* bbox) {
    glm::vec3 center = {(bbox->xmax + bbox->xmin) / 2,
        (bbox->ymax + bbox->ymin) / 2, (bbox->zmax + bbox->zmin) / 2};
    Cube cube = make_cube(center.x, center.y, center.z, bbox->xmax - center.x, bbox->ymax - center.y, bbox->zmax - center.y);
    return submit_primitive(&cube);
}

// For debugging purposes.
// TODO: height check is broken.
void submit_primitive(BVHTreeNode* root) {
    if (m_debug_bvh_height < 0) return;

    auto nodes = MakeSlice<BVHTreeNode*>(1);

    BVHTreeNode* stack[64];
    int stack_offset = 0;
    stack[stack_offset++] = root->right;
    stack[stack_offset++] = root->left;

    while (stack_offset > 0) {
        BVHTreeNode* node = stack[--stack_offset];
        if (stack_offset == m_debug_bvh_height) {
            append(&nodes, node);
        }

        if (node->left != NULL && node->right != NULL) {
            stack[stack_offset++] = node->right;
            stack[stack_offset++] = node->left;
        }
    }

    for (int i = 0; i < count(nodes); ++i) {
        submit_primitive(&nodes[i]->data.bbox);
    }
}

int64 submit_primitive(Chunk* chunk, SubmitFlags flags, int64) {
    // Non-exhaustive check to rule out non-triangle meshes:
    ph_assert(chunk->num_verts % 3 == 0);

    for (int64 i = 0; i < chunk->num_verts; i += 3) {
        glm::vec3 a, b, c;  // points
        glm::vec3 d, e, f;  // normals

        a = chunk->verts[i + 0];
        b = chunk->verts[i + 1];
        c = chunk->verts[i + 2];

        float sign = 1.0f - 2 * float((flags & SubmitFlags_FlipNormals) != 0);
        d = sign * chunk->norms[i + 0];
        e = sign * chunk->norms[i + 1];
        f = sign * chunk->norms[i + 2];

        GLtriangle tri;
        GLtriangle norm;
        tri.p0 = to_gl(a);
        tri.p1 = to_gl(b);
        tri.p2 = to_gl(c);
        norm.p0 = to_gl(d);
        norm.p1 = to_gl(e);
        norm.p2 = to_gl(f);

#ifdef PH_DEBUG
        auto vi = append(&m_triangle_pool, tri);
        auto ni = append(&m_normal_pool, norm);
        ph_assert(vi == ni);
#else
        append(&m_triangle_pool, tri);
        append(&m_normal_pool, norm);
#endif
    }

    ph_assert(chunk->num_verts / 3 < PH_MAX_int64);

    Primitive prim;
    prim.num_triangles = int(chunk->num_verts / 3);
    prim.offset = int(count(m_triangle_pool) - prim.num_triangles);
    prim.material = MaterialType_Lambert;
    auto index = append(&m_primitives, prim);

    return index;
}

Cube make_cube(float x, float y, float z, float size_x, float size_y, float size_z) {
    Cube c;
    c.center.x = x;
    c.center.y = y;
    c.center.z = z;

    c.sizes.x = size_x;
    c.sizes.y = size_y;
    c.sizes.z = size_z;

    return c;
}

Cube make_cube(float x, float y, float z, float size) {
    return make_cube(x,y,z,size,size,size);
}

void init() {
    static bool is_init = false;
    if (is_init) {
        clear(&m_triangle_pool);
        clear(&m_normal_pool);
        clear(&m_light_pool);
        clear(&m_primitives);
        update_structure();
        upload_everything();
    } else {
        m_triangle_pool = MakeSlice<GLtriangle>(1024);
        m_normal_pool   = MakeSlice<GLtriangle>(1024);
        m_light_pool    = MakeSlice<GLlight>(8);
        m_primitives    = MakeSlice<Primitive>(1024);

        glGenBuffers(1, &m_bvh_buffer);
        glGenBuffers(1, &m_triangle_buffer);
        glGenBuffers(1, &m_normal_buffer);
        glGenBuffers(1, &m_light_buffer);
        GLCHK ( glGenBuffers(1, &m_prim_buffer) );
    }
    is_init = true;

    // TODO: build a light system.
    Light light;
    light.data.position = {1, 0.5, -1, 1};
    submit_light(&light);
}

void update_structure() {
    ph_assert(count(m_primitives) < PH_MAX_int32);

    if (count(m_primitives) == 0) {
        return;
    }

    int32* indices = phalloc(int32, count(m_primitives));
    AABB* bbox_cache = phalloc(AABB, count(m_primitives));

    for (int i = 0; i < count(m_primitives); ++i) {
        indices[i] = i;
        bbox_cache[i] = get_bbox(&m_primitives[i], 1);
    }

    BVHTreeNode* root = build_bvh(m_primitives, indices, bbox_cache, 0);
    phree(indices);
    phree(bbox_cache);

#ifdef PH_DEBUG
    validate_bvh(root, m_primitives);
#endif

    int64 len;
    auto* flatroot = flatten_bvh(root, &len);

#ifdef PH_DEBUG
    validate_flattened_bvh(flatroot, len);
#endif

    // Upload flat bvh.
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bvh_buffer);
        GLCHK ( glBufferData(GL_SHADER_STORAGE_BUFFER,
                    GLsizeiptr(sizeof(BVHNode) * (size_t)len),
                    (GLvoid*)flatroot, GL_DYNAMIC_COPY) );
        GLCHK ( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_bvh_buffer) );

    }

    release(root);
    phree(flatroot);

}

void upload_everything() {
    // =========================  Upload to GPU
    // Submit triangle pool
    {
        GLCHK ( glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_triangle_buffer) );
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                GLsizeiptr(sizeof(scene::GLtriangle) * scene::m_triangle_pool.n_elems),
                (GLvoid*)scene::m_triangle_pool.ptr, GL_DYNAMIC_COPY);
        GLCHK ( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_triangle_buffer) );
    }

    // Submit normal pool
    {
        GLCHK ( glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_normal_buffer) );
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                GLsizeiptr(sizeof(scene::GLtriangle) * scene::m_normal_pool.n_elems),
                (GLvoid*)scene::m_normal_pool.ptr, GL_DYNAMIC_COPY);
        GLCHK ( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_normal_buffer) );
    }

    // Submit light data to gpu.
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_light_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                GLsizeiptr(sizeof(scene::GLlight) * scene::m_light_pool.n_elems),
                (GLvoid*)scene::m_light_pool.ptr, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_light_buffer);
    }

    // Submit primitive pool.
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_prim_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                GLsizeiptr(sizeof(scene::Primitive) * scene::m_primitives.n_elems),
                (GLvoid*)scene::m_primitives.ptr, GL_DYNAMIC_COPY);
        GLCHK ( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_prim_buffer) );
    }

}

} // ns scene
} // ns ph
