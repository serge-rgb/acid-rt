typedef struct {
    float3 point;
} Light;

typedef struct {
    float3 o;
    float3 d;
} Ray;

typedef struct {
    float4 orientation;  // Orientation quaternion.
    float3 position;
} Eye;

typedef struct {
    float t;
    float3 point;
    float3 norm;
    int depth;  // For debug heat map
} Intersection;

typedef struct {
    float3 p0;
    float3 p1;
    float3 p2;
    float3 _padding;
} Triangle;

typedef struct {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
    float zmin;
    float zmax;
} AABB;

typedef struct {
    int primitive_offset;       // >0 when leaf. -1 when not.
    int right_child_offset;     // Left child is adjacent to node. (-1 if leaf!)
    AABB bbox;
} BVHNode;

typedef struct {
    int offset;             // Num of elements into the triangle pool where this primitive begins.
    int num_triangles;
    int material;           // Enum (copy in shader).
} Primitive ;

inline float3 rotate_vector_quat(const float3 vec, const float4 quat) {
    float3 i = -quat.xyz;
    float m = quat.w;
    //return vec + 2.0 * cross( cross( vec, i ) + m * vec, i );
    return vec + 2.0 * cross( mad(m, vec, cross( vec, i )), i );
}

#define BBOX_USE_MAD
inline float bbox_collision(const AABB box, const Ray ray, const float3 inv_dir, float* far_t) {
    float t0;

    float rmin, rmax;
    float3 o = ray.o * inv_dir;

    float mx = -(o.x);
    float my = -(o.y);
    float mz = -(o.z);

#ifndef BBOX_USE_MAD
    rmin = box.xmin * inv_dir.x - o.x;
    rmax = box.xmax * inv_dir.x - o.x;
#else
    rmin = mad(box.xmin, inv_dir.x, mx);
    rmax = mad(box.xmax, inv_dir.x, mx);
#endif

    t0 = min(rmin, rmax);
    *far_t = max(rmin, rmax);

#ifndef BBOX_USE_MAD
    rmin = box.ymin * inv_dir.y - o.y;
    rmax = box.ymax * inv_dir.y - o.y;
#else
    rmin = mad(box.ymin, inv_dir.y, my);
    rmax = mad(box.ymax, inv_dir.y, my);
#endif

    t0 = max(t0, min(rmin, rmax));
    *far_t = min(*far_t, max(rmin, rmax));

#ifndef BBOX_USE_MAD
    rmin = box.zmin * inv_dir.z - o.z;
    rmax = box.zmax * inv_dir.z - o.z;
#else
    rmin = mad(box.zmin, inv_dir.z, mz);
    rmax = mad(box.zmax, inv_dir.z, mz);
#endif

    t0 = max(t0, min(rmin, rmax));
    *far_t = min(*far_t, max(rmin, rmax));

    return t0;
}

inline float3 barycentric(const Triangle tri, const Ray ray) {
    float3 e1 = tri.p1 - tri.p0;
    float3 e2 = tri.p2 - tri.p0;
    float3 s  = ray.o - tri.p0;
    float3 m  = cross(s, ray.d);
    float3 n = cross(e1, e2);
    float det = dot(-n, ray.d);
    //if (det <= EPSILON && det >= -EPSILON) return float3(-1);
    return (1 / det) * (float3)(dot(n, s), dot(m, e2), dot(-m, e1));
}

Intersection ray_sphere(const Ray* ray, const float3 c, const float r) {
    Intersection intersection;
    intersection.t = -1;
    const float b = dot(ray->d, ray->o - c);
    const float d = dot(ray->o - c, ray->o - c) - r * r;
    const float det = b*b - d;

    if (det >= 0) {
        intersection.t = -b + sqrt(det);
        intersection.point = ray->o + intersection.t * ray->d;
        intersection.norm = normalize(intersection.point - c);
    }

    return intersection;
}

Intersection trace(
        __constant BVHNode* nodes,
        __constant Primitive* prims,
        __constant Triangle* tris,
        __constant Triangle* norms,
        Ray ray) {
    Intersection its;
    its.depth = 0;
    float3 inv_dir = 1 / ray.d;

    int stack[32];
    int stack_offset = 0;
    int node_i = 0;
    BVHNode node = nodes[node_i];
    float min_t = 1 << 16;
    // while true
    // while node is internal
    //  traverse.
    // while node has primitives
    //  intesect
    while (true) {
        while (node.primitive_offset < 0) {  // Inner nodes.
            const AABB bbox_l = nodes[node_i + 1].bbox;
            const AABB bbox_r = nodes[node.right_child_offset].bbox;
            float nr, nl, fr, fl;
            nl = bbox_collision(bbox_l, ray, inv_dir, &fl);
            nr = bbox_collision(bbox_r, ray, inv_dir, &fr);

            bool hit_l = (nl < fl) && (nl < min_t) && (fl > 0);
            bool hit_r = (nr < fr) && (nr < min_t) && (fr > 0);

            node_i = node_i + 1;
            int other_i = node.right_child_offset;
            // If it hits just one
            if (hit_l != hit_r) {
                if (hit_r) {
                    node_i = other_i;
                }
            }
            // Either both or none
            else {
                if (!hit_l) {  // No hit.
                    if (stack_offset == 0) {
                        return its;
                    } else {
                        node_i = stack[--stack_offset];
                    }
                } else {  // Both hit
                    if (nr < nl) {
                        int tmp = node_i;
                        node_i = other_i;
                        other_i = tmp;
                    }
                    stack[stack_offset++] = other_i;
                }

            }

            node = nodes[node_i];
        }
        //============== LEAF =================
        Primitive prim = prims[node.primitive_offset];
        for (int j = 0; j < prim.num_triangles; ++j) {
            int offset = prim.offset + j;
            Triangle tri = tris[offset];

            float3 bar = barycentric(tri, ray);
            if (bar.x > 0 &&
                    bar.x < min_t &&
                    bar.y < 1 && bar.y > 0 &&
                    bar.z < 1 && bar.z > 0 &&
                    (bar.y + bar.z) < 1)
            {
                min_t = bar.x;
                Triangle norm = norms[offset];
                its.norm = (1 - bar.y - bar.z) * norm.p0 + bar.y * norm.p1 + bar.z * norm.p2;
                its.point = ray.o + bar.x * ray.d;
                its.t = bar.x;
            }
        }
        if (stack_offset == 0) {
            return its;
        }
        node_i = stack[--stack_offset];
        node = nodes[node_i];
    }
    return its;
}

float lambert(Light l, float3 point, float3 norm) {
    float3 dir = normalize(l.point - point);
    const float c = max(dot(norm, dir), (float)0);
    return c;
}


float catmull(float rsq, __constant float* K) {
    int num_segments = 11;

    /* float scaled_val = 10 * rsq / (max_r_sq); */
    // 3.6 == 100 * Distortion.Lens.MetersPerTanAngleAtCenter (DK2)
    float scaled_val = 10 * rsq * 3.6;
    float scaled_val_floor = max(0.0f, min(10.0f, floor(scaled_val)));

    int k = (int)(scaled_val_floor);
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
    float k0 = (float)(k == 0);

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

__kernel void main(
        __write_only image2d_t image,
        int x_off,
        float2 lens_center,
        float eye_to_screen,
        float2 viewport_size_m,
        int2 viewport_size_px,       // 5
        Eye eye,                     // 6
        __constant float* K,         // 7
        __constant Triangle* tris,   // 8
        __constant Triangle* norms,  // 9
        __constant Primitive* prims, // 10
        __constant BVHNode* nodes    // 11
        //
        ) {

    /* float4 color = 1; */
    float4 color = 0;

    float3 eye_pos = (float3)(0);
    float3 point = (float3)(
            (float)(get_global_id(0)) / viewport_size_px.x,
            (float)(get_global_id(1)) / viewport_size_px.y,
            0);

    // Point is in [0, 1] x [0, 1]

    // Translate to center ( in screen space. )
    point.xy -= lens_center / (float2)(viewport_size_m.x, viewport_size_m.y);

    // Correct for aspect ratio
    const float ar = (float)(viewport_size_px.y) / viewport_size_px.x;
    point.y *= ar;

    const float rsq = point.x * point.x + point.y * point.y;

    point *= (float3)(catmull(rsq, K));

    point.z -= eye_to_screen;

    // Rotate
    eye_pos = rotate_vector_quat(eye_pos, eye.orientation);
    point = rotate_vector_quat(point, eye.orientation);
    // Translate
    eye_pos += eye.position;
    point += eye.position;

    Ray ray;
    ray.o = (float3)point;
    ray.d = normalize(point - eye_pos);

    Light l;
    l.point = (float3)(-3,10,5);

    float min_t = 1 << 16;
    if (rsq < 0.25) {
        color = 0.0;

        Intersection its = trace(
                nodes,
                prims,
                tris,
                norms,
                ray);
        if (its.t > 0) {
            color = 1 * lambert(l, its.point, its.norm);
            color.x += (float)(its.depth) / 100.0f;
        }
    }

    size_t i, j;
    i = get_global_id(0) + x_off;
    j = get_global_id(1);
    write_imagef(image, (int2)(i,j), color);
}
