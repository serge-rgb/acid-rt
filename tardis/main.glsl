#version 430

layout(location = 0) uniform vec2 screen_size;  // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;
layout(location = 2) uniform float x_offset;        // In pixels, for separate viewports.
layout(location = 3) uniform float eye_to_lens_m;
layout(location = 4) uniform float sphere_y;        // TEST
layout(location = 5) uniform vec2 screen_size_m;    // In meters.
layout(location = 6) uniform vec2 lens_center_m;    // Lens center.
layout(location = 7) uniform vec4 orientation_q;    // Orientation quaternion.
layout(location = 8) uniform bool occlude;          // Flag for occlusion circle

// warp size: 64. (optimal warp size for my nvidia card)
layout(local_size_x = 8, local_size_y = 8) in;

vec3 rotate_vector_quat(vec3 vec, vec4 quat) {
    vec3 i = -quat.xyz;
    float m = quat.w;
    return vec + 2.0 * cross( cross( vec, i ) + m * vec, i );
}

struct Ray {
    vec3 o;
    vec3 dir;
};

struct Plane {
    vec3 normal;
    vec3 point;
};

struct Rect {
    Plane plane;
    vec2 size;
};

struct Sphere {
    float r;
    vec3 center;
// TODO: add inside factor
};

struct Light {
    vec3 position;
    vec3 color;
};

////////////////////////////////////////
// Collision structs
// Collision       -- Hit or not.
// Collision_point -- Hit, with point
////////////////////////////////////////

struct Collision {
    bool exists;
};

struct CollisionFull {
    bool exists;
    vec3 point;
};

Collision sphere_collision_p(Sphere s, Ray r) {
    vec3 dir = r.dir;
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
    }
    return coll;
}

CollisionFull sphere_collision(Sphere s, Ray r) {
    vec3 dir = r.dir;
    CollisionFull coll;
    coll.exists = false;

    float A = dot(dir, dir);
    float B = 2 * dot(dir, s.center);
    float C = dot(s.center, s.center) - (s.r * s.r);
    float disc = (B*B - 4*A*C);
    if (disc < 0.0) {
        return coll;
    }
    float t = (-B + sqrt(disc)) / (2 * A);
    if (t >= 0) {  // TODO: t has to be negative? What happens to all the diverging rays?
        return coll;
    }
    coll.exists = true;
    coll.point = r.o + (r.dir * t);
    return coll;
}

Collision plane_collision_p(Plane p, Ray r) {
    Collision coll;
    coll.exists = false;
    float disc = dot(r.dir, p.normal);
    if (disc > 0) {
        return coll;
    }
    coll.exists = true;
    return coll;
}

CollisionFull plane_collision(Plane p, Ray r) {
    CollisionFull coll;
    coll.exists = false;
    float disc = dot(r.dir, p.normal);
    if (disc > 0) {
        return coll;
    }
    coll.exists = true;

    float t = dot((p.point - r.o), p.normal);

    coll.exists = true;
    coll.point = r.o + (t * r.dir);

    return coll;
}

Collision rect_collision_p(Rect rect, Ray r) {
    Collision coll = plane_collision_p(rect.plane, r);
    if (!coll.exists) { //
        return coll;
    }
    return coll;
}

// ========================================
// Material functions.
// ========================================

vec3 lambert(vec3 point, vec3 normal, vec3 color, Light l) {  // Light should not be a parameter. All lights should contribute. -- Right?
    return color * dot(normal, normalize(l.position - point));
}

float barrel(float r) {
    float k0 = 1.0;
    float k1 = 300;
    float k2 = 800.0;
    float k3 = 0.0;
    return k0 + r * (k1 + r * ( r * k2 + r * k3));
}

void main() {
    //////
    // global
    //////
    float z_eye = 0.0;
    Sphere s;
    s.r = 0.1;
    s.center = vec3(0, sphere_y + 1, -0.5);

    Plane p;
    p.normal = vec3(0, 1, 0);
    p.point  = vec3(0, 0, 0);

    Rect r;
    r.plane.normal = vec3(0, 0, 1);
    r.plane.point  = vec3(0);
    r.size         = vec2(0.1, 0.1);

    Light l;
    l.position = vec3(0, 100, 0);
    // l.color    = vec3(1, 0.5, 0.5);
    l.color    = vec3(1,1,1);

    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);

    //float ar = screen_size.y / screen_size.x;
    // The eye is a physically accurate position (in meters) of the ... ey
    vec3 eye = vec3(0, 0, eye_to_lens_m);

    // Rotate eye
    // ....
    eye = rotate_vector_quat(eye, orientation_q);

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

    // Radius squared. Used for culling and distortion correction.
    // get it before we rotate everything..
    float radius_sq = (point.x * point.x) + (point.y * point.y);

    point = rotate_vector_quat(point, orientation_q);

    vec3 dir = point - eye;  // View direction

    vec4 color;  // This ends up written to the image.

    if (occlude && radius_sq > 0.0016) {         // <--- Cull
        color = vec4(0);
    } else {                                     // <--- Ray trace.
        Ray ray;
        ray.o = point * barrel(radius_sq);
        ray.dir = (ray.o - eye);

        CollisionFull c = plane_collision(p, ray);
        if (c.exists) {
            color = vec4(lambert(c.point, p.normal, vec3(0,0.5,0), l), 1);
        } else {
            color = vec4(0.1, 0.1, 0.5, 1);
        }

        CollisionFull cf = sphere_collision(s, ray);
        if (cf.exists) {
            color = vec4(-lambert(cf.point, normalize(cf.point - s.center), vec3(0,0,1), l), 1);
        }
    }

    imageStore(tex, coord, color);
}

