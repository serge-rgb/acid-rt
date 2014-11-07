typedef struct Light_s {
    float3 point;
} Light;

typedef struct Ray_s {
    float3 o;
    float3 d;
} Ray;

typedef struct Intersection_s {
    float t;
    float3 point;
    float3 norm;
} Intersection;


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

// TODO: impl
float catmull(float rsq) {
    return 1.0;
}

__kernel void main(
        __write_only image2d_t image,
        int x_off,
        float2 lens_center,
        float eye_to_screen,
        float2 viewport_size_m,
        int2 viewport_size_px) {

    float4 color = 1;

    float3 eye = (float3)(0);
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

    point *= (float4)catmull(rsq);

    point.z -= eye_to_screen;


    // TODO: rotation & translation

    Ray ray;
    ray.o = (float3)point;
    ray.d = normalize(point - eye);

    Light l;
    l.point = (float3)(0,10,-5);

    if (rsq < 0.25) {
        color = 0;

        Intersection its = ray_sphere(&ray, (float3)(0,0,-2), 0.2);
        if (its.t > 0) {
            color = (float4)(1,1,1,1);
            float f = lambert(l, its.point, its.norm);
            color *= f;
        }
    }

    size_t i, j;
    i = get_global_id(0) + x_off;
    j = get_global_id(1);
    write_imagef(image, (int2)(i,j), color);
}
