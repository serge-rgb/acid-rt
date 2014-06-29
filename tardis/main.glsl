#version 430

layout(location = 0) uniform vec2 screen_size;  // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;
layout(location = 2) uniform float x_offset;
layout(location = 3) uniform float eye_to_lens_m;
layout(location = 4) uniform float sphere_y;

// warp size: 64. (optimal warp size for my nvidia card)
layout(local_size_x = 8, local_size_y = 8) in;

struct Sphere {
    float r;
    vec3 center;
};

struct Collision {
    bool exists;
    vec3 color;
};

Collision sphere_collision(Sphere s, vec3 dir) {
    Collision coll;
    coll.exists = false;

    float A = dot(dir, dir);
    float B = 2 * dot(dir, s.center);
    float C = dot(s.center, s.center) - (s.r * s.r);
    float disc = (B*B - 4*A*C);
    if (disc < 0.0) {
        return coll;
    } else { // Hit!
        coll.exists = true;
        coll.color = vec3(2*dir.r,0,dir.b);
    }
    return coll;
}

void main() {
    //////
    // global
    //////
    float z_eye = 0.0;


    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);

    //vec4 color = vec4(vec2(gl_GlobalInvocationID.xy)/vec2(screen_size.x, screen_size.y), 0, 1.0);

    float ar = screen_size.y / screen_size.x;
    // The eye is a physically accurate position (in meters) of the ... ey
    vec3 eye = vec3(0, 0, eye_to_lens_m);
    // This point represents the pixel in the viewport as a point in the frustrum near face
    vec3 point = vec3(2 * (gl_GlobalInvocationID.x / screen_size.x) - 1,
                      2 * (gl_GlobalInvocationID.y / screen_size.y) - 1,
                      0);
    point.y *= ar;
    point.x += eye.x;  // Point is moved to eye position
    // TODO: scale point to actual viewport size

    vec4 color;
    if (distance(point.xy, vec2(0)) > 1.0) {    // <--- Cull
        color = vec4(0);
    } else {                                    // <--- Ray trace.
        Sphere s;
        s.r = 0.4;
        s.center = vec3(0, sphere_y, -2.5);
        vec3 dir = point - eye;
        Collision c = sphere_collision(s, dir);
        if (c.exists) {
            color = vec4(c.color,1);
        } else {
            color = vec4(abs(point.x), abs(point.y), 0, 1);
        }
    }

    imageStore(tex, coord, color);
}

