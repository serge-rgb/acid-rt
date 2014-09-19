#version 430

layout(location = 0) uniform vec2 screen_size;          // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;     // This is the image we write to.
layout(location = 2) uniform float x_offset;            // In pixels, for separate viewports.
layout(location = 3) uniform float eye_to_lens_m;
// Not going to use this while it is 1.0
//layout(location = 4) uniform float max_r_sq;            // Max radius squared (for catmull spline).
layout(location = 5) uniform vec2 screen_size_m;        // Screen size in meters.
layout(location = 6) uniform vec2 lens_center_m;        // Lens center.
layout(location = 7) uniform vec4 orientation_q;        // Orientation quaternion.
layout(location = 8) uniform bool occlude;              // Flag for occlusion circle
// 9 unused
layout(location = 10) uniform vec3 camera_pos;

float PI = 3.141526;
float EPSILON = 0.00001;
float INFINITY = 1 << 16;

// Sync this with enum in C codebase.
int MaterialType_Lambert = 0;

vec3 rotate_vector_quat(vec3 vec, vec4 quat) {
    vec3 i = -quat.xyz;
    float m = quat.w;
    return vec + 2.0 * cross( cross( vec, i ) + m * vec, i );
}

struct Ray {
    vec3 o;
    vec3 dir;
};

struct Triangle {
    vec3 p0;
    vec3 p1;
    vec3 p2;
    vec3 normal;
};

struct Primitive {
    int offset;  // Into triangle_pool
    int num_triangles;
    int material;
};

struct AABB {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
    float zmin;
    float zmax;
};

struct BVHNode {
    int primitive_offset;
    int r_child_offset;
    AABB bbox;
};

struct Light {
    vec3 position;
    vec3 color;
};

struct TraceIntersection {
    float t;
    vec3 point;
    vec3 normal;
};

layout(std430, binding = 0) buffer TrianglePool {
    Triangle data[];
} triangle_pool;

layout(std430, binding = 1) buffer LightPool {
    Light data[];
} light_pool;

layout(std430, binding = 2) buffer PrimitivePool {
    Primitive data[];
} primitive_pool;

layout(std430, binding = 3) buffer BVH {
    BVHNode data[];
} bvh;

float bbox_collision(AABB box, Ray ray, inout bool is_inside) {
    // Perf note:
    //  Precomputing inv_dir gives no measurable perf gain (geforce 770)
    // vec3 inv_dir = vec3(1) / ray.dir;
    float t0 = 0;
    float t1 = INFINITY;
    float xmin, xmax, ymin, ymax, zmin, zmax;

    xmin = (box.xmin - ray.o.x) / ray.dir.x;
    xmax = (box.xmax - ray.o.x) / ray.dir.x;

    t0 = min(xmin, xmax);
    t1 = max(xmin, xmax);

    ymin = (box.ymin - ray.o.y) / ray.dir.y;
    ymax = (box.ymax - ray.o.y) / ray.dir.y;

    t0 = max(t0, min(ymin, ymax));
    t1 = min(t1, max(ymin, ymax));

    zmin = (box.zmin - ray.o.z) / ray.dir.z;
    zmax = (box.zmax - ray.o.z) / ray.dir.z;

    t0 = max(t0, min(zmin, zmax));
    t1 = min(t1, max(zmin, zmax));

    is_inside = t0 <= 0;

    float collides = float(t0 < t1);
    /* return collides * t0 + (1 - collides) * (-INFINITY); */
    return collides * t1 + (1 - collides) * (-INFINITY);

    /* if (t0 < t1) { */
        /* return t0;// > 0? t0 : t1; */
    /* } else { */
    /*     return -INFINITY; */
    /* } */
}

vec3 barycentric(Triangle tri, Ray ray) {
    vec3 e1 = tri.p1 - tri.p0;
    vec3 e2 = tri.p2 - tri.p0;
    vec3 s  = ray.o - tri.p0;
    vec3 m  = cross(s, ray.dir);
    vec3 n = cross(e1, e2);
    float det = dot(-n, ray.dir);
    //if (det <= EPSILON && det >= -EPSILON) return vec3(-1);
    return (1 / det) * vec3(dot(n, s), dot(m, e2), dot(-m, e1));
}

TraceIntersection trace(Ray ray) {
    TraceIntersection intersection;
    float min_t = INFINITY;
    vec3 point;
    vec3 normal;
    vec2 uv;

    int stack[16];
    int stack_offset = 0;
    stack[stack_offset++] = 0;
    while (stack_offset > 0) {
        int i = stack[--stack_offset];
        BVHNode node = bvh.data[i];
        bool is_inside; // is inside bbox?
        float bbox_t = bbox_collision(node.bbox, ray, is_inside);
        bool ditch_node = !is_inside && bbox_t > min_t;    // TODO: fix this
        if (node.primitive_offset >= 0 && !ditch_node) {                     // LEAF
            Primitive p = primitive_pool.data[node.primitive_offset];
            for (int j = p.offset; j < p.offset + p.num_triangles; ++j) {
                Triangle t = triangle_pool.data[j];
                vec3 bar = barycentric(t, ray);
                if (bar.x > 0 &&
                        bar.y < 1 && bar.y > 0 &&
                        bar.z < 1 && bar.z > 0 &&
                        (bar.y + bar.z) < 1) {
                    if (bar.x < min_t) {
                        min_t = bar.x;
                        float u = bar.y;
                        float v = bar.z;
                        point = ray.o + bar.x * ray.dir;
                        //point = (1 - u - v) * t.p0 + u * t.p1 + v * t.p2;
                        normal = t.normal;
                        uv = vec2(u,v);
                    }
                }
            }
        } else if (bbox_t > 0 && !ditch_node) {                              // INNER NODE
            stack[stack_offset++] = i + 1;
            stack[stack_offset++] = node.r_child_offset;
        }
    }
    intersection.t = min_t;
    intersection.point = point;
    intersection.normal = normal;
    return intersection;
}

// ========================================
// Material functions.
// ========================================

vec3 lambert(vec3 point, vec3 normal, vec3 color, Light l) {
    //return normal;
    float d = max(dot(normal, normalize(l.position - point)), 0);
    return color * d;
}

float barrel(float r) {
    float k0 = 1.0;
    float k1 = 0.22;
    float k2 = 0.24;
    float k3 = 0.0;
    return k0 + r * (k1 + r * ( r * k2 + r * k3));
}

float recip_poly(float r) {
    /* float k0 = 1.0; */
    /* float k1 = -0.494165344f; */
    /* float k2 = 0.587046423f; */
    /* float k3 = -0.841887126f; */
    float m = 1.32928;
    float k0 = 1.0;
    float k1 = m * 0.4;
    float k2 = m * 0.8;
    float k3 = m * 1.5;
    return 1 / (k0 + r * (k1 + r * (r * k2 + r * k3)));
}

    const float K[11] = {
        1.003000,
        1.020000,
        1.042000,
        1.066000,
        1.094000,
        1.126000,
        1.162000,
        1.203000,
        1.250000,
        1.310000,
        1.380000,
    };  // TODO: Hard coded, because Oculus hard codes it ATM (0.4.2)

float catmull(float r) {
    int num_segments = 11;

    /* float scaled_val = 10 * r / (max_r_sq); */
    float scaled_val = 10 * r * 2;
    float scaled_val_floor = max(0.0f, min(10, floor(scaled_val)));

    int k = int(scaled_val_floor);
    float p0, p1, k0p0, k0p1;
    float m0, m1, k0m0, k0m1;

    //==== k == 0
    k0p0 = 1.0f;
    k0m0 = K[1] - K[0];
    k0p1 = K[1];
    k0m1 = 0.5f * (K[2] - K[0]);
    //====

    //==== 0 < k < 9
    p0 = K[k];
    m0 = 0.5 * (K[k + 1] - K[k - 1]);
    p1 = K[k + 1];
    m1 = 0.5 * (K[k + 2] - K[k]);
    //====

    // k >= 9 does not happen for DK2 and would be expensive. Ignore it

    // Pseudo if
    float k0 = float(k == 0);

    p0 = k0 * k0p0 + (1 - k0) * p0;
    p1 = k0 * k0p1 + (1 - k0) * p1;
    m0 = k0 * k0m0 + (1 - k0) * m0;
    m1 = k0 * k0m1 + (1 - k0) * m1;

    float t = scaled_val - scaled_val_floor;
    float omt = 1 - t;
    float res  = ( p0 * ( 1.0f + 2.0f * t ) + m0 *   t ) * omt * omt +
        ( p1 * ( 1.0f + 2.0f * omt ) - m1 * omt ) *   t *   t;
    return res;

}

// (x * y) % 32 == 0
layout(local_size_x = 16, local_size_y = 8) in;
void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);

    //float ar = screen_size.y / screen_size.x;
    // The eye is a physically accurate position (in meters) of the ... ey
    vec3 eye = vec3(0, 0, 0);

    // This point represents the pixel in the viewport as a point in the frustrum near face
    vec3 point = vec3((gl_GlobalInvocationID.x / screen_size.x),
                      (gl_GlobalInvocationID.y / screen_size.y),
                      0);

    // Point is in [0,1]x[0,1]

    // Convert unit to meters
    point.xy *= screen_size_m;

    // Center the point at zero (lens center)
    point.xy -= lens_center_m.xy;

    // back to unit coordinates
    point.xy /= screen_size_m;

    // Get radius squared
    float radius_sq = (point.x * point.x) + (point.y * point.y);

    // Scale by aspect ratio.
    point.x *= screen_size.x / screen_size.y;

    // Distortion correction
    point *= catmull(radius_sq);

    // Separate point and eye
    point.z -= eye_to_lens_m;

    // Rotate.
    eye = rotate_vector_quat(eye, orientation_q);
    point = rotate_vector_quat(point, orientation_q);

    // Camera movement
    eye += camera_pos;
    point += camera_pos;

    vec4 color;  // This ends up written to the image.

    if (occlude && radius_sq > 0.25) {         // <--- Cull
        // Green is great for checking appropiate radius.
        /* color = vec4(0,1,0,1); */
        color = vec4(0);
    } else {                                     // <--- Ray trace.
        Ray ray;
        ray.o = point;
        ray.dir = ray.o - eye;

        color = vec4(0.5);
        color.a = 1;
        TraceIntersection intersection = trace(ray);
        float t = intersection.t;
        // --- actual trace ends here

        if (t < INFINITY) {
            color = vec4(0);
            int num_lights = light_pool.data.length();
            for (int i = 0; i < num_lights; ++i) {
                Light light = light_pool.data[i];
                /* light.position = vec3(0,100,0); */
                vec3 rgb = (1.0 / num_lights) *
                    lambert(intersection.point, intersection.normal, vec3(0.9, 0.9, 0.9), light);
                /* vec3 rgb = point; */
                color += vec4(rgb, 1);
            }

        }
    }


    imageStore(tex, coord, color);
}

