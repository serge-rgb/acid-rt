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
} Intersection;

typedef struct {
    float3 p0;
    float3 p1;
    float3 p2;
} Triangle;

inline float3 rotate_vector_quat(const float3 vec, const float4 quat) {
    float3 i = -quat.xyz;
    float m = quat.w;
    return vec + 2.0 * cross( cross( vec, i ) + m * vec, i );
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
        int2 viewport_size_px,      // 5
        Eye eye,                    // 6
        __constant float* K,        // 7
        __constant Triangle* tris,  // 8
        __constant Triangle* norms, // 9
        int num_tris
        //
        ) {

    float4 color = 1;

    float3 eye_pos = (float3)(0);
    float3 point = (float3)(
            (float)(get_global_id(0)) / viewport_size_px.x,
            (float)(get_global_id(1)) / viewport_size_px.y,
            0);

    // Point is in [0, 1] x [0, 1]

    // Translate to center ( in screen space. )
    point.xy -= lens_center / (float2)(viewport_size_m.x, viewport_size_m.y / 2);

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
    l.point = (float3)(0,10,-5);

    if (rsq < 0.25) {
        color = 0.0;

        for (int i = 0; i < num_tris; ++i) {
            Triangle tri = tris[i];

            Intersection its = ray_sphere(&ray, (float3)tri.p0, 0.2);
            if (its.t > 0) {
                color = 0.9;
                float f = lambert(l, its.point, its.norm);
                color *= f;
                color += (float4)(0.1);
            }
            its = ray_sphere(&ray, (float3)tri.p1, 0.2);
            if (its.t > 0) {
                color = 0.9;
                float f = lambert(l, its.point, its.norm);
                color *= f;
                color += (float4)(0.1);
            }
            its = ray_sphere(&ray, (float3)tri.p2, 0.2);
            if (its.t > 0) {
                color = 0.9;
                float f = lambert(l, its.point, its.norm);
                color *= f;
                color += (float4)(0.1);
            }
            float3 bar = barycentric(tri, ray);
            if (bar.x > 0 &&
                    bar.y < 1 && bar.y > 0 &&
                    bar.z < 1 && bar.z > 0 &&
                    (bar.y + bar.z) < 1
                    /*&& bar.x < min_t*/)
            {
                color = 1;
            }
        }
    }

    size_t i, j;
    i = get_global_id(0) + x_off;
    j = get_global_id(1);
    write_imagef(image, (int2)(i,j), color);
}