__kernel void fill_checkerboard(__write_only image2d_t image, int x_off) {
    /* const   sampler_t sampler = */
    /*     CLK_ADDRESS_CLAMP | */
    /*     //CLK_ADDRESS_NONE | */
    /*     CLK_NORMALIZED_COORDS_TRUE | */
    /*     CLK_FILTER_NEAREST; */
    size_t i, j;
    i = get_global_id(0) + x_off;
    j = get_global_id(1);

    int m = (get_group_id(0) + get_group_id(1)) % 2;
    write_imagef(image, (int2)(i,j), (float4)(m));
}
