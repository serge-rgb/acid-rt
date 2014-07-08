#version 430

layout(location = 0) uniform vec2 screen_size;          // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;     // This is the image we write to.
layout(location = 2) uniform float x_offset;            // In pixels, for separate viewports.
layout(location = 3) uniform float eye_to_lens_m;       //
layout(location = 5) uniform vec2 screen_size_m;        // In meters.
layout(location = 6) uniform vec2 lens_center_m;        // Lens center.
layout(location = 7) uniform vec4 orientation_q;        // Orientation quaternion.
layout(location = 8) uniform bool occlude;              // Flag for occlusion circle
// 9 unused
layout(location = 10) uniform vec2 camera_pos;          //
// warp size: 64. (optimal warp size for my nvidia card)
layout(local_size_x = 8, local_size_y = 8) in;
void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);
    imageStore(tex,coord, vec4(0,1,0,1));
}
