#version 430

layout(location = 0) uniform vec2 screen_size;  // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;
layout(location = 2) uniform float x_offset;
layout(location = 3) uniform float eye_to_lens_m;
layout(location = 4) uniform float sphere_y;
layout(location = 5) uniform vec2 screen_size_m;  // In meters.
layout(location = 6) uniform vec2 lens_center_m;  // Lens center.


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
    Sphere s;
    s.r = 0.1;
    s.center = vec3(0, sphere_y, -0.5);


    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);

    //float ar = screen_size.y / screen_size.x;
    // The eye is a physically accurate position (in meters) of the ... ey
    vec3 eye = vec3(0, 0, eye_to_lens_m);

    // This point represents the pixel in the viewport as a point in the frustrum near face
    vec3 point = vec3((gl_GlobalInvocationID.x / screen_size.x),
                      (gl_GlobalInvocationID.y / screen_size.y),
                      0);

    // Point is in [0,1]x[0,1].
    // We need to convert it to meters relative to the screen.
    point.xy *= screen_size_m;

    // Center the point at zero (lens center)
    point.x -= lens_center_m.x;
    point.y -= lens_center_m.y;

    vec3 dir = normalize(point - eye);  // View direction

    vec4 color;  // This ends up written to the image.

    // Radius squared. Used for culling and distortion correction.
    float radius_sq = (point.x * point.x) + (point.y * point.y);

    if (radius_sq > 0.0016) {                      // <--- Cull
        color = vec4(0);
    } else {                                    // <--- Ray trace.
        Collision c = sphere_collision(s, dir);
        if (c.exists) {
            color = vec4(c.color,1);
        } else {
            color = 10 * vec4(abs(point.x), abs(point.y), 0, 1);
        }
    }
    //point *= 10;
    //color = vec4(abs(point),1);

    imageStore(tex, coord, color);
}

