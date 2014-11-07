
struct CBColors {
    float a[4];
    float b[4];
};

struct Ray {
    float3 o;
    float3 d;
};


float ray_sphere(const struct Ray* ray, const float3 c, const float3 r) {
    return 0;
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

    float4 color;
    /////////////////
    // Checkerboard
    int m = (get_group_id(0) + get_group_id(1)) % 2;
    if (m == 0) {
        color = (float4)(0,0,0,1);
    } else {
        color = (x_off == 960)? (float4)(1,1,1,1) : (float4)(0,0,0.3,1);
    }
    ////////////////

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

    struct Ray ray;
    ray.o = (float3)point;
    ray.d = normalize(point - eye);

    if (rsq < 0.25) {
        color = (float4)(ray.d, 1.0f);
    }

    size_t i, j;
    i = get_global_id(0) + x_off;
    j = get_global_id(1);
    write_imagef(image, (int2)(i,j), color);
}
