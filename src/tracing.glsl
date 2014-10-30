#version 430

layout(location = 0) uniform vec2 screen_size;              // One eye! (960, 1080 for DK2)
//layout(location = 1) writeonly uniform image2D tex;       // This is the image we write to.
layout(location = 1, rgba32f) uniform image2D tex;          // This is the image we write to.
layout(location = 2) uniform float x_offset;                // In pixels, for separate viewports.
layout(location = 3) uniform float eye_to_lens_m;
// Not going to use this while it is 1.0
//layout(location = 4) uniform float max_r_sq;              // Max radius squared (for catmull spline).
layout(location = 5) uniform vec2 screen_size_m;            // Screen size in meters.
layout(location = 6) uniform vec2 lens_center_m;            // Lens center.
layout(location = 7) uniform vec4 orientation_q;            // Orientation quaternion.
layout(location = 8) uniform bool occlude;                  // Flag for occlusion circle
layout(location = 9) uniform int frame_index;               //
layout(location = 10) uniform vec3 camera_pos;
layout(location = 11, rgba32f) uniform image2D back_tex;    // Framebuffer for the prev frame
layout(location = 12) uniform bool enable_interlacing;      // Should warp interlacing optimization be on.
layout(location = 13) uniform samplerCube sky;              // Skybox texture
layout(location = 14) uniform bool skybox_enabled;

#extension GL_NV_shadow_samplers_cube : enable


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
    int l_child_offset;
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
    int debug;
};

layout(std430, binding = 0) buffer TrianglePool {
    readonly Triangle data[];
} triangle_pool;

layout(std430, binding = 1) buffer LightPool {
    readonly Light data[];
} light_pool;

layout(std430, binding = 2) buffer PrimitivePool {
    readonly Primitive data[];
} primitive_pool;

layout(std430, binding = 3) buffer BVH {
    readonly BVHNode data[];
} bvh;

layout(std430, binding = 4) buffer NormalPool {
    readonly Triangle data[];
} normal_pool;

float bbox_collision(AABB box, Ray ray, vec3 inv_dir, out float far_t) {
    // Note
    //  Rearranging into (x * a) + b gives a minor perf gain (MUL+ADD => MAD ??)
    float t0 = 0;
    float t1 = INFINITY;

    float rmin, rmax;
    ray.o *= inv_dir;

    rmin = box.xmin * inv_dir.x - ray.o.x;
    rmax = box.xmax * inv_dir.x - ray.o.x;

    t0 = min(rmin, rmax);
    t1 = max(rmin, rmax);

    rmin = box.ymin * inv_dir.y - ray.o.y;
    rmax = box.ymax * inv_dir.y - ray.o.y;

    t0 = max(t0, min(rmin, rmax));
    t1 = min(t1, max(rmin, rmax));

    rmin = box.zmin * inv_dir.z - ray.o.z;
    rmax = box.zmax * inv_dir.z - ray.o.z;

    t0 = max(t0, min(rmin, rmax));
    t1 = min(t1, max(rmin, rmax));

    far_t = t1;
    return t0;
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
    intersection.t = INFINITY;
    float min_t = INFINITY;

    int stack[16];
    int stack_offset = 0;

    vec3 inv_dir = vec3(1) / ray.dir;

    BVHNode node = bvh.data[0];
    int cnt = 0;
    while (true) {
        if (cnt > 3000) break;
        cnt++;
        if (node.primitive_offset >= 0) {                     // LEAF
            Primitive p = primitive_pool.data[node.primitive_offset];
            for (int j = p.offset; j < p.offset + p.num_triangles; ++j) {
                Triangle t = triangle_pool.data[j];
                vec3 bar = barycentric(t, ray);
                if (bar.x > 0 &&
                        bar.y < 1 && bar.y > 0 &&
                        bar.z < 1 && bar.z > 0 &&
                        (bar.y + bar.z) < 1) {
                    if (bar.x < min_t) {
                        Triangle n = normal_pool.data[j];
                        min_t = bar.x;
                        float u = bar.y;
                        float v = bar.z;
                        //point = (1 - u - v) * t.p0 + u * t.p1 + v * t.p2;
                        intersection.t = min_t;
                        intersection.point = ray.o + bar.x * ray.dir;
                        intersection.normal = (1 - u - v) * n.p0 + u * n.p1 + v * n.p2;
                    }
                }
            }
        } else {                              // INNER NODE
            // Get left and right
            BVHNode left = bvh.data[node.l_child_offset];
            BVHNode right = bvh.data[node.r_child_offset];
            int other = node.r_child_offset;
            int other_l = node.l_child_offset;


            float l_n, l_f, r_n, r_f;
            l_n = bbox_collision(left.bbox, ray, inv_dir, l_f);
            r_n = bbox_collision(right.bbox, ray, inv_dir, r_f);

            bool hit_l = (l_n < l_f) && (l_n < min_t) && (l_f > 0);
            bool hit_r = (r_n < r_f) && (r_n < min_t) && (r_f > 0);

            // If anyone got hit
            if ((hit_l || hit_r)) {
                // If just one... traverse
                node = hit_l ? left : right;

                // When *both* children are hits choose the nearest
                if (hit_l && hit_r) {
                    float near = min(l_n, r_n);
                    if (near == r_n) {
                        node = right;
                        other = other_l;
                    }

                    // We will need to traverse the other node later.
                    stack[stack_offset++] = other;
                }
                continue;
            }
        }
        // Reaching this only when there was no hit. Get from stack.
        if (stack_offset > 0) {
            node = bvh.data[stack[--stack_offset]];
        } else {
            return intersection;
        }
    }
    intersection.t = INFINITY;
    intersection.debug = 1;
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
};  // TODO: Hard coded,

float catmull(float r) {
    int num_segments = 11;

    /* float scaled_val = 10 * r / (max_r_sq); */
    float scaled_val = 10 * r * 3.6;  // 3.6 == 100 * Distortion.Lens.MetersPerTanAngleAtCenter (DK2)
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
layout(local_size_x = 10, local_size_y = 16) in;
//layout(local_size_x = 1, local_size_y = 256) in;
void main() {
    int off = int(frame_index % 2);
    bool quit = (off + (gl_WorkGroupID.x + gl_WorkGroupID.y)) % 2 == 0;
    quit = quit && enable_interlacing;
    // This is will happen with "stretched warps" because the height is not divisible by 128
    if (gl_GlobalInvocationID.y > screen_size.y) { quit = true; }
    //quit = false;
    if (frame_index != -1 && quit) {
        ivec2 coord_store = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);
        ivec2 coord_load = coord_store + ivec2(0 * screen_size.x, 0);

        vec4 color;
        //color = imageLoad(back_tex, coord_load);  // TODO: back_tex has sampling problems.
        color = imageLoad(tex, coord_load);
        /* vec4 color = imageLoad(back_tex, coord_store); */
        imageStore(tex, coord_store, color);
        return;
    }

    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);

    //float ar = screen_size.y / screen_size.x;
    vec3 eye = vec3(0, 0, 0);

    vec3 sky_coord;
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

    // At this stage, point is close to the un-oriented texture coordinate for the skybox.
    // The constant 1.22222222 is (110 / 90).
    //  A typical cubemap face has a 90 degree vertical FOV.
    //  This is a hacky fix for 110 degree FOV. :)
    sky_coord = normalize(vec3(point.xy,-1) * 1.22222222);

    // Scale by aspect ratio.
    point.x *= screen_size.x / screen_size.y;

    // Get radius squared
    float radius_sq = (point.x * point.x) + (point.y * point.y);

    // Distortion correction
    point *= catmull(radius_sq);

    // Separate point and eye
    point.z -= eye_to_lens_m;

    // Rotate.
    eye = rotate_vector_quat(eye, orientation_q);
    point = rotate_vector_quat(point, orientation_q);
    sky_coord = rotate_vector_quat(sky_coord, vec4(orientation_q.xyz, orientation_q.w));

    // Camera movement
    eye += camera_pos;
    point += camera_pos;

    vec4 color;  // This ends up written to the image.

    if (occlude && radius_sq > 0.18) {         // <--- Cull
        // Green is great for checking appropiate radius.
        /* color = vec4(0,1,0,1); */
        color = vec4(0);
    } else {                                     // <--- Ray trace.
        Ray ray;
        ray.o = point;
        ray.dir = ray.o - eye;  // Not normalized so that we get correct distances. FWIW

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
                color.rgb += vec4(rgb, 1).rgb;
            }
        } else {
            if (skybox_enabled) color = textureCube(sky, normalize(sky_coord));
        }
        if (intersection.debug == 1) {
            color.rgb = vec3(0);
            color.r = 1;
        }
    }

    imageStore(tex, coord, color);
}

