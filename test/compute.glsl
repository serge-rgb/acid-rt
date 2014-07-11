#version 430

layout(location = 0) uniform vec2 screen_size;  // Passing half width here by default
layout(location = 1) writeonly uniform image2D tex;


// warp size: 32.
layout(local_size_x = 8, local_size_y = 8) in;
void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = vec4(vec2(gl_GlobalInvocationID.xy)/vec2(2 * screen_size.x, screen_size.y), 0, 1.0);

    imageStore(tex, coord, color);
}
