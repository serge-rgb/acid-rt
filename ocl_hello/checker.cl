
struct CBColors {
    float a[4];
    float b[4];
};

__kernel void fill_checkerboard(__write_only image2d_t image, int x_off,
        struct CBColors colors) {
    size_t i, j;
    i = get_global_id(0) + x_off;
    j = get_global_id(1);

    int m = (get_group_id(0) + get_group_id(1)) % 2;
    float4 color;
    if (m == 0) {
        color = (float4)(colors.a[0], colors.a[1], colors.a[2], colors.a[3]);
    } else {
        color = (float4)(colors.b[0], colors.b[1], colors.b[2], colors.b[3]);
    }
    write_imagef(image, (int2)(i,j), color);
}
