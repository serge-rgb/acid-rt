// 2014 Sergio Gonzalez

#include <ph.h>


namespace ph {

const char* str(const glm::vec3& v) {
    char* out = phanaged(char, 16);
    sprintf(out, "%f, %f, %f", v.x, v.y, v.z);
    return out;
}

namespace scene {
struct AABB;
const char* str(scene::AABB b);
}

////////////////////////////////////////
//          -- ph::vr --
// Handle the creation / destruction
// of Oculus resources.
// Members should provide everything
// needed by higher level abstractions
// to handle rendering and so on.
////////////////////////////////////////
namespace vr {

static const OVR::HMDInfo* m_hmdinfo;
static OVR::HmdRenderInfo* m_renderinfo;
static ovrHmd              m_hmd;
static float               m_default_eye_z;  // Getting the default FOV, calculate eye distance from plane.

void init() {
    // Safety net.
    static bool is_init = false;
    if (is_init) {
        phatal_error("vr::init called twice");
    }
    is_init = true;
    ///////////////
    // Allocate crap
    // Not deallocated. This should only be called once or the caller is dumb and ugly.
    m_renderinfo = new OVR::HmdRenderInfo();
    ///////////////

    if (!ovr_Initialize()) {
        ph::phatal_error("Could not initialize OVR\n");
    }
    m_hmd = ovrHmd_Create(0);
    ovrHmdDesc desc;
    ovrHmd_GetDesc(m_hmd, &desc);

    ovrHmd_StartSensor(m_hmd, 0x1111/*all*/, ovrHmd_GetEnabledCaps(m_hmd));

    auto fovPort_l = desc.DefaultEyeFov[0];
    auto fovPort_r = desc.DefaultEyeFov[1];

    ovrEyeRenderDesc rdesc[2];

    rdesc[0] = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Left, fovPort_l);
    rdesc[1] = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Right, fovPort_r);

    hmdState* state = (hmdState*)m_hmd;
    m_hmdinfo = &state->HMDInfo;

    // Default fov (looking down)
    float hvfov = (fovPort_r.DownTan + fovPort_l.DownTan) / 2.0f;
    float h = m_hmdinfo->ScreenSizeInMeters.h;

    // Let's get the eye distance.
    m_default_eye_z = h / (1 * hvfov);

    *m_renderinfo = GenerateHmdRenderInfoFromHmdInfo(*m_hmdinfo);

}

void deinit() {
    ovrHmd_StopSensor(m_hmd);
    ovrHmd_Destroy(m_hmd);
    ovr_Shutdown();
}

}  // ns vr


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
namespace scene {

// For DFS buffers.
static const int kTreeStackLimit = 64;

// Defined below
struct GLtriangle;
struct GLlight;
struct Primitive;

static Slice<GLtriangle>    m_triangle_pool;
static Slice<GLlight>       m_light_pool;
static Slice<Primitive>     m_primitives;
static int                  m_debug_bvh_height = -1;

enum SubmitFlags {
    SubmitFlags_None = 1 << 0,
    SubmitFlags_FlipNormals
};

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
    GLvec3 normal;
};

struct GLlight {
    GLvec3 position;
    GLvec3 color;
};

struct Light {
    GLlight data;
    int64 index;
};

struct Cube {
    glm::vec3 center;
    glm::vec3 sizes;
    int index;          //  Place in triangle pool where associated triangles begin.
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

struct AABB {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
    float zmin;
    float zmax;
};

const char* str(AABB b) {
    char* out = phanaged(char, 16);
    sprintf(out, "%f, %f\n%f, %f\n%f, %f\n", b.xmin, b.xmax, b.ymin, b.ymax, b.zmin, b.zmax);
    return out;
}

static glm::vec3 get_centroid(AABB b) {
    return glm::vec3((b.xmax + b.xmin) / 2, (b.ymax + b.ymin) / 2, (b.zmax + b.zmin) / 2);
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
            GLvec3 points[3] = {tri.p0, tri.p1, tri.p2};
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

////////////////////////////////////////
// BVH Accel
////////////////////////////////////////

// Plain and simple struct for flattened tree.
struct BVHNode {
    int primitive_offset;       // >0 when leaf. -1 when not.
    int right_child_offset;     // Left child is adjacent to node. (-1 if leaf!)
    AABB bbox;
    // Alignment of box: 6*32 bits, which aligns to 8 * 32 bits.
    // With two extra ints, we need 6 bytes of padding for GL alignment.
    int _padding[6];
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
static BVHTreeNode* build_bvh(Slice<Primitive> primitives, const int* indices) {
    BVHTreeNode* node = phanaged(BVHTreeNode, 1);
    BVHNode data;
    data.primitive_offset = -1;
    data.right_child_offset = -1;

    ph_assert(count(primitives) != 0);
    data.bbox = get_bbox(primitives.ptr, (int)count(primitives));

    if (count(primitives) == 1) {           // ---- Leaf
        data.primitive_offset = *indices;
        node->left  = NULL;
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

        float variation[3] = {0, 0, 0};  // Used to choose split axis
        for (int i = 0; i < count(centroids) - 1; ++i) {
            variation[0] += fabs(centroids[i].x - centroids[i + 1].x);
            variation[1] += fabs(centroids[i].y - centroids[i + 1].y);
            variation[2] += fabs(centroids[i].z - centroids[i + 1].z);
        }

        SplitPlane split = SplitPlane_X;
        { // Choose split plane
            float v = -1;
            for (int i = 0; i < 3; i++) {
                if (v < variation[i]) {
                    v = variation[i];
                    split = SplitPlane(i);
                }
            }
            if (v == 0.0f) {
                printf ("Two objets have the same centroid. Fix that, or fix me.\n");
            }
            ph_assert ( v > 0 );
        }

        // Make two new slices.
        auto slice_left  = MakeSlice<Primitive>(size_t(count(primitives) / 2));
        auto slice_right = MakeSlice<Primitive>(size_t(count(primitives) / 2));
        // These indices keep the old ordering.
        int* new_indices_l = phalloc(int, (size_t)count(primitives));
        int* new_indices_r = phalloc(int, (size_t)count(primitives));
        int offset_l = 0;
        int offset_r = 0;
        memcpy(new_indices_l, indices, sizeof(int) * (size_t)count(primitives));
        memcpy(new_indices_r, indices, sizeof(int) * (size_t)count(primitives));

        // Partition two slices based on which side of the midpoint.
        for (int i = 0; i < count(primitives); ++i) {

            auto centroid = centroids[i];

            switch (split) {
            case SplitPlane_X:
                {
                    if (centroid.x < midpoint.x) {
                        append(&slice_left, primitives[i]);
                        new_indices_l[offset_l++] = indices[i];
                    }
                    if (centroid.x >= midpoint.x) {
                        append(&slice_right, primitives[i]);
                        new_indices_r[offset_r++] = indices[i];
                    }
                    break;
                }
            case SplitPlane_Y:
                {
                    if (centroid.y < midpoint.y) {
                        append(&slice_left, primitives[i]);
                        new_indices_l[offset_l++] = indices[i];
                    }
                    if (centroid.y >= midpoint.y) {
                        append(&slice_right, primitives[i]);
                        new_indices_r[offset_r++] = indices[i];
                    }
                    break;
                }
            case SplitPlane_Z:
                {
                    if (centroid.z < midpoint.z) {
                        append(&slice_left, primitives[i]);
                        new_indices_l[offset_l++] = indices[i];
                    }
                    if (centroid.z >= midpoint.z) {
                        append(&slice_right, primitives[i]);
                        new_indices_r[offset_r++] = indices[i];
                    }
                    break;
                }
            }
        }

        node->left = node->right = NULL;
        if (count(slice_left)) {
            node->left = build_bvh(slice_left, new_indices_l);
        }
        if (count(slice_right)) {
            node->right = build_bvh(slice_right, new_indices_r);
        }
        phree(new_indices_l);
        phree(new_indices_r);
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
            stack[stack_offset++] = node->right;
            stack[stack_offset++] = node->left;
            if (stack_offset > height) {
                height = stack_offset;
            }
        }
    }

    for (int i = 0; i < count(data); ++i) {
        if(checks[i] == false) {
            printf("Leaf %d not found.\n", i);
            return false;
        }
    }

    phree (checks);
    printf("BVH valid.\n");
    return true;
}

// Returns a memory-managed array of BVHNode in depth first order. Ready for GPU consumption.
BVHNode* flatten_bvh(BVHTreeNode* root, int64* out_len) {
    assert(root->left && root->right);

    auto slice = MakeSlice<BVHNode>(1024);          // Too big?
    auto ptrs  = MakeSlice<BVHTreeNode*>(1024);     // Use this to get offsets for right childs (too slow?)

    BVHTreeNode* stack[kTreeStackLimit];
    int stack_offset = 0;
    stack[stack_offset++] = root;

    while(stack_offset > 0) {
        auto fatnode = stack[--stack_offset];
        BVHNode node = fatnode->data;
        append(&slice, node);
        append(&ptrs, fatnode); // Slow? push pointer...
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
            int64 found_i = find(ptrs, fatnode->right);
            ph_assert(found_i >= 0);
            ph_assert(found_i < int64(1) << 31);
            slice.ptr[i].right_child_offset = (int)found_i;
        }
        i++;
    }

    *out_len = count(slice);
    return slice.ptr;
}

bool validate_flattened_bvh(BVHNode* node, int64 len) {
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
    printf("Flat tree valid.\n");
    return true;
}

// Return a vec3 with layout expected by the compute shader.
// Reverse z while we're at it, so it is in view coords.
static GLvec3 to_gl(glm::vec3 in) {
    GLvec3 out = {in.x, in.y, in.z, 0};
    return out;
}

void submit_light(Light* light) {
    light->index = append(&m_light_pool, light->data);
}

void submit_primitive(Cube* cube, SubmitFlags flags = SubmitFlags_None) {
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

    // Return index to the first appended triangle.
    int64 index = -1;

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
    tri.normal = nf;
    index = append(&m_triangle_pool, tri);
    tri.p0 = h;
    tri.p1 = e;
    tri.p2 = b;
    tri.normal = nf;
    append(&m_triangle_pool, tri);

    // Right
    tri.p0 = e;
    tri.p1 = c;
    tri.p2 = b;
    tri.normal = nr;
    append(&m_triangle_pool, tri);
    tri.p0 = e;
    tri.p1 = c;
    tri.p2 = f;
    tri.normal = nr;
    append(&m_triangle_pool, tri);

    // Back
    tri.p0 = d;
    tri.p1 = c;
    tri.p2 = g;
    tri.normal = nb;
    append(&m_triangle_pool, tri);
    tri.p0 = c;
    tri.p1 = f;
    tri.p2 = g;
    tri.normal = nb;
    append(&m_triangle_pool, tri);

    // Left
    tri.p0 = a;
    tri.p1 = h;
    tri.p2 = d;
    tri.normal = nl;
    append(&m_triangle_pool, tri);
    tri.p0 = h;
    tri.p1 = d;
    tri.p2 = g;
    tri.normal = nl;
    append(&m_triangle_pool, tri);

    // Top
    tri.p0 = a;
    tri.p1 = c;
    tri.p2 = d;
    tri.normal = nt;
    append(&m_triangle_pool, tri);
    tri.p0 = a;
    tri.p1 = b;
    tri.p2 = c;
    tri.normal = nt;
    append(&m_triangle_pool, tri);

    // Bottom
    tri.p0 = h;
    tri.p1 = f;
    tri.p2 = g;
    tri.normal = nm;
    append(&m_triangle_pool, tri);
    tri.p0 = h;
    tri.p1 = e;
    tri.p2 = f;
    tri.normal = nm;
    append(&m_triangle_pool, tri);

    ph_assert(index <= long(1) << 31);
    cube->index = (int)index;
    append(&m_primitives, {cube->index, 12, MaterialType_Lambert});
}

void submit_primitive(AABB* bbox) {
    glm::vec3 center = {(bbox->xmax + bbox->xmin) / 2,
        (bbox->ymax + bbox->ymin) / 2, (bbox->zmax + bbox->zmin) / 2};
    Cube cube = {center, {bbox->xmax - center.x, bbox->ymax - center.y, bbox->zmax - center.y}, -1};
    submit_primitive(&cube);
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


void init() {
    m_triangle_pool = MakeSlice<GLtriangle>(1024);
    m_light_pool    = MakeSlice<GLlight>(8);
    m_primitives    = MakeSlice<Primitive>(1024);

    double float_scale = 10;
    Cube room = {{0,0,-1.5}, {float_scale, float_scale, float_scale}, -1};
    submit_primitive(&room, SubmitFlags_FlipNormals);

    Cube floor = {{0,-0.6,-2}, {2, 0.1, 2}, -1};
    submit_primitive(&floor);

    Cube top = {{0,4,-2}, {2, 0.1, 2}, -1};
    submit_primitive(&top);

    Cube thing;
    {
        int x = 4;
        int y = 1;
        int z = 1;
        for (int i = 0; i < x; ++i) {
            for (int j = 0; j < y; ++j) {
                for (int k = 0; k < z; ++k) {
                    thing = {{i * 1.1, j * 1.1, -2 - k * 1.1}, {0.5, 0.5, 0.5}, -1};
                    submit_primitive(&thing);
                }
            }
        }
    }

    // Do this after submitting everything:
    ph_assert(count(m_primitives) < long(1) << 31)

    int* indices = phalloc(int, count(m_primitives));
    for (int i = 0; i < count(m_primitives); ++i) {
        indices[i] = i;
    }
    BVHTreeNode* root = build_bvh(m_primitives, indices);
    phree(indices);

    validate_bvh(root, m_primitives);

    int64 len;
    auto* flatroot = flatten_bvh(root, &len);

    validate_flattened_bvh(flatroot, len);

    submit_primitive(root);  // Debug BVH submitting...

    Light light;
    light.data.position = {1, 0.5, -1, 1};
    submit_light(&light);
    /* light.data.position = {1, 1, -2, 1}; */
    /* submit_light(&light); */
    /* light.data.position = {-9, 0, -2, 1}; */
    /* submit_light(&light); */
    /* light.data.position = {0, 9, -2, 1}; */
    /* submit_light(&light); */

    /* submit_light(&light); */

}

} // ns scene


////////////////////////////////////////
//          -- tardis --
//  Entry point for the game.
////////////////////////////////////////

static GLuint g_program;
static int g_size[] = {1920, 1080};
/* static int g_size[] = {1280, 800}; */
/* static int g_size[] = {640, 400}; */
// Note: perf is really sensitive about this. Runtime tweak?
// Note 2: Workgroup size should be a multiple of workgroup size.
static int g_warpsize[] = {16, 8};
static GLfloat g_viewport_size[2];

enum {
    Control_W = 1 << 0,
    Control_A = 1 << 1,
    Control_S = 1 << 2,
    Control_D = 1 << 3,
};

static int pressed = 0;

static void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    if (key == GLFW_KEY_W && action == GLFW_PRESS) {
       pressed |= Control_W;
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS) {
       pressed |= Control_A;
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
       pressed |= Control_S;
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
       pressed |= Control_D;
    }
    if (key == GLFW_KEY_W && action == GLFW_RELEASE) {
       pressed &= ~Control_W;
    }
    if (key == GLFW_KEY_A && action == GLFW_RELEASE) {
       pressed &= ~Control_A;
    }
    if (key == GLFW_KEY_S && action == GLFW_RELEASE) {
       pressed &= ~Control_S;
    }
    if (key == GLFW_KEY_D && action == GLFW_RELEASE) {
       pressed &= ~Control_D;
    }
    if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
        if (scene::m_debug_bvh_height >= 0) {
            scene::m_debug_bvh_height--;
        }
    }
    if (key == GLFW_KEY_9 && action == GLFW_PRESS) {
        scene::m_debug_bvh_height++;
    }
}

void init(GLuint prog) {
    // Eye-to-lens
    glUseProgram(prog);
    float eye_to_lens_m = vr::m_default_eye_z; // Calculated for default FOV
    glUniform1f(3, eye_to_lens_m);

    g_viewport_size[0] = GLfloat (g_size[0]) / 2;
    g_viewport_size[1] = GLfloat (g_size[1]);
    GLfloat size_m[2] = {
        vr::m_renderinfo->ScreenSizeInMeters.w / 2,
        vr::m_renderinfo->ScreenSizeInMeters.h,
    };
    glUniform2fv(5, 1, size_m);     // screen_size_m
    glUniform1f(8, true);           // Occlude?

    // TODO: cs::scene should handle the actual submit.
    //  especially regarding triangles.

    // Submit triangle pool
    {
        GLuint triangle_buffer;
        glGenBuffers(1, &triangle_buffer);
        GLCHK ( glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangle_buffer) );
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                    GLsizeiptr(sizeof(scene::GLtriangle) * scene::m_triangle_pool.n_elems),
                    (GLvoid*)scene::m_triangle_pool.ptr, GL_DYNAMIC_COPY);
        GLCHK ( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triangle_buffer) );
    }

    // Submit light data to gpu.
    {
        GLuint light_buffer;
        glGenBuffers(1, &light_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                GLsizeiptr(sizeof(scene::GLlight) * scene::m_light_pool.n_elems),
                (GLvoid*)scene::m_light_pool.ptr, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, light_buffer);
        GLCHK();
    }

    // Submit primitive pool.
    {
        GLuint prim_buffer;
        GLCHK ( glGenBuffers(1, &prim_buffer) );
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, prim_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                GLsizeiptr(sizeof(scene::Primitive) * scene::m_primitives.n_elems),
                (GLvoid*)scene::m_primitives.ptr, GL_DYNAMIC_COPY);
        GLCHK ( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, prim_buffer) );
    }

}

void draw() {
    GLCHK ( glUseProgram(g_program) );

    static GLfloat sphere_y = 0.0f;
    static float step_var = 0.0;
    sphere_y = 0.2f * sinf(step_var);
    glUniform1f(4, sphere_y);
    step_var += 0.05;

    static unsigned int frame_index = 1;
    if (frame_index == 1) {
        ovrHmd_BeginFrameTiming(vr::m_hmd, frame_index);
    }

    ovrFrameTiming frame_timing = ovrHmd_BeginFrame(vr::m_hmd, frame_index);
    frame_timing = ovrHmd_GetFrameTiming(vr::m_hmd, frame_index);

    auto sdata = ovrHmd_GetSensorState(vr::m_hmd, frame_timing.ScanoutMidpointSeconds);

    ovrPosef pose = sdata.Predicted.Pose;

    auto q = pose.Orientation;
    GLfloat quat[4] {
        q.x, q.y, q.z, q.w,
    };
    glUniform4fv(7, 1, quat);

    static GLfloat camera_pos[2] = {0, 0};
    static GLfloat cam_step = 0.03f;
    auto glm_q = glm::quat(quat[0], quat[1], quat[2], quat[3]);
    auto axis = glm::axis(glm_q);
    auto e = glm::vec3(0, 0, 1);

    glm::vec3 rotated_e = e + 2.0f * glm::cross(glm::cross(e, axis) + glm_q.w * e, axis);

    GLfloat cam_step_x;
    GLfloat cam_step_y;
    cam_step_x = cam_step * rotated_e.z;
    cam_step_y = cam_step * rotated_e.x;

    if (pressed & Control_W) {
        camera_pos[0] -= cam_step_x;
        camera_pos[1] -= cam_step_y;
    }
    if (pressed & Control_S) {
        camera_pos[0] += cam_step_x;
        camera_pos[1] += cam_step_y;
    }

    e = glm::vec3(1, 0, 0);
    rotated_e = e + 2.0f * glm::cross(glm::cross(e, axis) + glm_q.w * e, axis);
    cam_step_x = cam_step * rotated_e.z;
    cam_step_y = cam_step * rotated_e.x;

    if (pressed & Control_A) {
        camera_pos[0] += cam_step_x;
        camera_pos[1] += cam_step_y;
    }
    if (pressed & Control_D) {
        camera_pos[0] -= cam_step_x;
        camera_pos[1] -= cam_step_y;
    }
    glUniform2fv(10, 1, camera_pos);  // update camera_pos

    GLCHK ( glUseProgram(g_program) );
    // Dispatch left viewport
    {
        GLfloat lens_center[2] = {
            (vr::m_renderinfo->ScreenSizeInMeters.w / 2) -
                (vr::m_renderinfo->LensSeparationInMeters / 2),
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, 0);  // x_offset
        GLCHK ( glDispatchCompute(GLuint(g_viewport_size[0] / g_warpsize[0]),
                    GLuint(g_viewport_size[1] / g_warpsize[1]), 1) );
    }
    // Dispatch right viewport
    {
        GLfloat lens_center[2] = {
            vr::m_renderinfo->LensSeparationInMeters / 2,
            vr::m_hmdinfo->CenterFromTopInMeters,
        };
        glUniform2fv(6, 1, lens_center);  // Lens center
        glUniform1f(2, ((GLfloat)g_viewport_size[0]));  // x_offset
        GLCHK ( glDispatchCompute(GLuint(g_viewport_size[0] / g_warpsize[0]),
                    GLuint(g_viewport_size[1] / g_warpsize[1]), 1) );
    }
    frame_index++;
    ovrHmd_EndFrame(vr::m_hmd);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    // Draw screen
    cs::fill_screen();
}

} // ph

using namespace ph;
int main() {
    ph::init();
    vr::init();

    window::init("Project TARDIS", g_size[0], g_size[1],
            window::InitFlag(window::InitFlag_NoDecoration |
                window::InitFlag_OverrideKeyCallback));

    glfwSetKeyCallback(ph::window::m_window, key_callback);

    const char* paths[] = {
        "tardis/main.glsl",
    };

    g_program = cs::init(g_size[0], g_size[1], paths, 1);

    scene::init();

    init(g_program);

    window::draw_loop(draw);

    window::deinit();

    vr::deinit();
    return 0;
}
