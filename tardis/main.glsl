#version 430

layout(location = 0) uniform vec2 screen_size;  // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;
layout(location = 2) uniform float x_offset;

// warp size: 64. (optimal warp size for my nvidia card)
layout(local_size_x = 8, local_size_y = 8) in;
void main() {
    //////
    // global
    //////
    float z_eye = 0.0;
    vec3 center = vec3(x_offset + (screen_size.x / 2), screen_size.y / 2, z_eye);


    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);

    //vec4 color = vec4(vec2(gl_GlobalInvocationID.xy)/vec2(screen_size.x, screen_size.y), 0, 1.0);

    // The eye is a physically accurate position (in meters) of the ... ey
    vec3 eye = vec3(0, screen_size.y, -1);
    // This point represents the pixel in the viewport as a point in the frustrum near face
    vec3 point = vec3(2 * (gl_GlobalInvocationID.x / screen_size.x) - 1,
                     // dividing by x is intentional. aspect ratio is important
                      2 * (gl_GlobalInvocationID.y / screen_size.x) - 1,
                      0);
    point.x += eye.x;  // Point is moved to eye position

    // Trippy visualization
    vec4 color = vec4(abs(point.x), abs(point.y), 0, 1);

    imageStore(tex, coord, color);
}
