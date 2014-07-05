#version 430

layout(location = 0) uniform vec2 screen_size;        // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;   // This is the image we write to.
layout(location = 2) uniform float x_offset;          // In pixels, for separate viewports.
layout(location = 3) uniform float eye_to_lens_m;
layout(location = 4) uniform float sphere_y;          // TEST
layout(location = 5) uniform vec2 screen_size_m;      // In meters.
layout(location = 6) uniform vec2 lens_center_m;      // Lens center.
layout(location = 7) uniform vec4 orientation_q;      // Orientation quaternion.
layout(location = 8) uniform bool occlude;            // Flag for occlusion circle
layout(location = 9) uniform float curr_compression;  // Space compression from our perspective.


float PI = 3.141526;
float EPSILON = 0.00000000;

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

struct Triangle {
    vec3 p0;
    vec3 p1;
    vec3 p2;
};

struct Rect {
    vec3 a,b,c,d;
};

struct Cube {
    vec3 a,b,c,d,e,f,g,h;
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

vec3 normal_for_rect(Rect r) {
    return normalize(cross(r.a - r.b, r.d - r.b));
}

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
    float t;
};

struct CollisionCube {
    bool exists;
    vec3 point;
    vec3 normal;
    float t;
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
    coll.t = 0;

    float A = dot(dir, dir);
    float B = 2 * dot(dir, s.center);
    float C = dot(s.center, s.center) - (s.r * s.r);
    float disc = (B*B - 4*A*C);
    if (disc < 0.0) {
        return coll;
    }
    float t = (B - sqrt(disc)) / (2 * A);
    if (t <= 0) {
        return coll;
    }
    coll.exists = true;
    coll.point = r.o + (r.dir * t);
    coll.t = t;
    return coll;
}

vec3 barycentric(Ray ray, Triangle tri) {
    vec3 e1 = tri.p1 - tri.p0;
    vec3 e2 = tri.p2 - tri.p0;
    vec3 d = ray.dir;
    vec3 q = cross(d, e2);
    float det = dot(q, e1);
    vec3 s  = ray.o - tri.p0;
    vec3 r = cross(s, e1);
    // Optimization note:
    //   Brancing kills perf.
    //   Defining unused vars before the if stmt is also bad.
    /* if (det == 0) { */
    /*     return vec3(-1,0,0); */
    /* } */
    return float(det == 0) * vec3(0) +
            (1 - float(det == 0)) * (1 / det) * vec3(dot(r, e2),
                                                    dot(q, s),
                                                    dot(r, d));
}


CollisionFull rect_collision(Rect rect, Ray r) {
    CollisionFull coll;
    coll.exists = false;

    Triangle t0, t1;
    t0.p0 = rect.a;
    t0.p1 = rect.b;
    t0.p2 = rect.c;

    t1.p0 = rect.a;
    t1.p1 = rect.c;
    t1.p2 = rect.d;

    vec3 b_coord0;
    vec3 b_coord1;
    float z;
    b_coord0 = barycentric(r, t0);
    b_coord1 = barycentric(r, t1);

    z = 1 - b_coord0.y - b_coord0.z;
    float check0 = float
            (b_coord0.x > 0 &&
            b_coord0.y <= 1.0 && b_coord0.y >= 0 &&
            b_coord0.z <= 1.0 && b_coord0.z >= 0 &&
            z <= 1 && z >= 0);
    z = 1 - b_coord1.y - b_coord1.z;
    float check1 = float
            (b_coord1.x > 0 &&
            b_coord1.y <= 1.0 && b_coord1.y >= 0 &&
            b_coord1.z <= 1.0 && b_coord1.z >= 0 &&
            z <= 1 && z >= 0);

    coll.exists = bool(check0) || bool(check1);

    if (coll.exists)
    {
        coll.t = check0 * b_coord0.x + check1 * b_coord1;
        coll.point = r.o + coll.t * r.dir;
    }

    return coll;
}

CollisionCube cube_collision(Cube c, Ray ray) {
    CollisionCube coll;
    coll.exists = false;
    Rect r;
    float min_t = 1 << 16;
    for (int i = 0; i < 6; ++i) {
        switch (i) {
            case 0: // Front
                {
                    r.a = c.a;
                    r.b = c.e;
                    r.c = c.f;
                    r.d = c.b;
                }
                break;
            case 1: // Left
                {
                    r.a = c.b;
                    r.b = c.f;
                    r.c = c.g;
                    r.d = c.c;
                }
                break;
            case 2: // Back
                {
                    r.a = c.c;
                    r.b = c.g;
                    r.c = c.h;
                    r.d = c.d;
                }
                break;
            case 3: // Right
                {
                    r.a = c.d;
                    r.b = c.h;
                    r.c = c.e;
                    r.d = c.a;
                }
                break;
            case 4: // Top
                {
                    r.a = c.a;
                    r.b = c.b;
                    r.c = c.c;
                    r.d = c.d;
                }
                break;
            case 5: // Bottom
                {
                    r.a = c.e;
                    r.b = c.h;
                    r.c = c.g;
                    r.d = c.f;
                }
                break;
        }
        CollisionFull _coll = rect_collision(r, ray);
        if (_coll.exists && _coll.t < min_t) {
            min_t = _coll.t;
            coll.exists = true;
            coll.point = _coll.point;
            coll.normal = normal_for_rect(r);
            coll.t = _coll.t;
        }
    }
    return coll;
}

// ========================================
// Material functions.
// ========================================

// Light should not be a parameter. All lights should contribute. -- Right?
vec3 lambert(vec3 point, vec3 normal, vec3 color, Light l) {
    //return normal;
    return color * dot(normal, normalize(l.position - point));
}

float barrel(float r) {
    float k0 = 1.0;
    float k1 = 300;
    float k2 = 800.0;
    float k3 = 0.0;
    return k0 + r * (k1 + r * ( r * k2 + r * k3));
}

////////////////////////////////////////
// Primitive creation funcs.
////////////////////////////////////////
Cube MakeCube(vec3 center, vec3 size) {
    Cube c;
    c.a = vec3(-size.x, size.y , -size.z) + center;
    c.b = vec3(size.x, size.y , -size.z) + center;
    c.c = vec3(size.x, size.y , size.z) + center;
    c.d = vec3(-size.x, size.y , size.z) + center;
    c.e = vec3(-size.x, -size.y , -size.z) + center;
    c.f = vec3(size.x, -size.y , -size.z) + center;
    c.g = vec3(size.x, -size.y , size.z) + center;
    c.h = vec3(-size.x, -size.y , size.z) + center;
    return c;
}

Cube rotate_cube(Cube c, vec4 q) {
    c.a = rotate_vector_quat(c.a, q);
    c.b = rotate_vector_quat(c.b, q);
    c.c = rotate_vector_quat(c.c, q);
    c.d = rotate_vector_quat(c.d, q);
    c.e = rotate_vector_quat(c.e, q);
    c.f = rotate_vector_quat(c.f, q);
    c.g = rotate_vector_quat(c.g, q);
    c.h = rotate_vector_quat(c.h, q);
    return c;
}

void main() {
    //////
    // global
    //////

    Light l;
    l.position = vec3(20, 20, 20);
    l.position = vec3(0, 1, 1);
    l.color    = vec3(1,1,1);

    Sphere s;
    s.r = 0.3;
    s.center = vec3(1*sphere_y, 1, -1);

    Rect f;
    float fw = 0.3;
    f.a = vec3(-fw, -0.1, -fw);
    f.b = vec3(fw,  -0.1, -fw);
    f.c = vec3(fw,  -0.1, fw);
    f.d = vec3(-fw, -0.1, fw);

    Cube c = MakeCube(vec3(0,0,-0.5), vec3(0.1));
    c = rotate_cube(c, (sphere_y, vec4(sphere_y, 0,1,0)));

    Cube room = MakeCube(vec3(0,0,0), vec3(50));

    ivec2 coord = ivec2(gl_GlobalInvocationID.x + x_offset, gl_GlobalInvocationID.y);

    //float ar = screen_size.y / screen_size.x;
    // The eye is a physically accurate position (in meters) of the ... ey
    vec3 eye = vec3(0, 0, eye_to_lens_m);

    // The compression of the volume we are currently in.
    eye.z /= curr_compression;


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

    // ... we need to take into account the current compression.
    point.xy /= curr_compression;
    // Center the point at zero (lens center)
    point.x -= lens_center_m.x / curr_compression;
    point.y -= lens_center_m.y / curr_compression;

    // Radius squared. Used for culling and distortion correction.
    // get it before we rotate everything..
    // Normalize to curr_compression because barrel distortion is hard-wired to physical screen size.
    float radius_sq = curr_compression * curr_compression * ((point.x * point.x) + (point.y * point.y));

    point = rotate_vector_quat(point, orientation_q);

    vec4 color;  // This ends up written to the image.

    if (occlude && radius_sq > 0.0016) {         // <--- Cull
        color = vec4(0);
    } else {                                     // <--- Ray trace.
        Ray ray;
        ray.o = point * barrel(radius_sq);
        ray.dir = ray.o - eye;

        float min_t = 1 << 16;    // Z-buffer substitute

        color = vec4(1, 1, 1, 1);

        CollisionFull cs = sphere_collision(s, ray);
        if (true && cs.exists && min_t > cs.t) {
            min_t = cs.t;
            color = vec4(lambert(cs.point, normalize(cs.point - s.center), vec3(0,0,1), l), 1);
        }

        CollisionFull cr;
        for (int i = 0; i < 1000; ++i)
        cr = rect_collision(f, ray);
        if (cr.exists && min_t > cr.t) {
            min_t = cr.t;
            color = vec4(lambert(cr.point, normal_for_rect(f), vec3(1), l), 0.8);
        }

        CollisionCube cc = cube_collision(c, ray);
        if (cc.exists && min_t > cc.t) {
            min_t = cc.t;
            color = vec4(lambert(cc.point, cc.normal, vec3(0.99,0.5,0.5), l), 1);
        }
        cc = cube_collision(room, ray);
        if (cc.exists && min_t > cc.t) {
            min_t = cc.t;
            color = vec4(lambert(cc.point, -cc.normal, vec3(0.4, 0.4, 0.4), l), 1);
        }
        // TODO: Deal with possible negative min_t;
    }

    imageStore(tex, coord, color);
}

