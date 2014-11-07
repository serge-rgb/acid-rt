
struct CBColors {
    float a[4];
    float b[4];
};

struct Ray {
    float3 o;
    float3 d;
};

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

    struct Ray ray;
    ray.o = (float3)(0);

    float3 point =
        (float3)(
                (float)(get_global_id(0)) / viewport_size_px.x,
                (float)(get_global_id(1)) / viewport_size_px.y,
                0);

    point.xy -= lens_center / viewport_size_m;

    // Aspect ratio
    point.x *= viewport_size_px.x / viewport_size_px.y;
    //point.y *= viewport_size_px.y / viewport_size_px.x;

    float rsq = point.x * point.x + point.y * point.y;

    point *= (float4)catmull(rsq);

    point.z -= eye_to_screen;


    // TODO: rotation & translation
    ray.o.z += 1.0;

    ray.d = normalize(point - ray.o);


    if (rsq < 0.18) {
        color = (float4)(point, 1.0f);
    }

    size_t i, j;
    i = get_global_id(0) + x_off;
    j = get_global_id(1);
    write_imagef(image, (int2)(i,j), color);
}
