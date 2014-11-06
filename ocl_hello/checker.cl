__kernel void fill_checkerboard(__write_only image2d_t image) {
    /* const   sampler_t sampler = */
    /*     CLK_ADDRESS_CLAMP | */
    /*     //CLK_ADDRESS_NONE | */
    /*     CLK_NORMALIZED_COORDS_TRUE | */
    /*     CLK_FILTER_NEAREST; */
    size_t i, j;
    i = get_global_id(0);
    j = get_global_id(1);
    int2 coord = (i,j);
    write_imagef(image, coord, 0.5);
}
