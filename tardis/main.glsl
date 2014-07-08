#version 430

layout(location = 0) uniform vec2 screen_size;          // One eye! (960, 1080 for DK2)
layout(location = 1) writeonly uniform image2D tex;     // This is the image we write to.
layout(location = 2) uniform float x_offset;            // In pixels, for separate viewports.
layout(location = 3) uniform float eye_to_lens_m;       //
layout(location = 4) uniform float sphere_y;            // TEST
layout(location = 5) uniform vec2 screen_size_m;        // In meters.
layout(location = 6) uniform vec2 lens_center_m;        // Lens center.
layout(location = 7) uniform vec4 orientation_q;        // Orientation quaternion.
layout(location = 8) uniform bool occlude;              // Flag for occlusion circle
// 9 unused
layout(location = 10) uniform vec2 camera_pos;          //

float PI = 3.141526;
float EPSILON = 0.00000000;

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
    vec3 normal;
};

layout(std430, binding = 0) buffer TrianglePool {
    Triangle data[];
} triangle_pool;

struct Rect {
    vec3 a,b,c,d;
};

struct Cube {
    vec3 center;
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
    return normalize(cross(r.a - r.b, r.c - r.b));
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
    vec3 q = cross(ray.dir, e2);
    float det = dot(q, e1);
    vec3 s  = ray.o - tri.p0;
    vec3 r = cross(s, e1);
    // Optimization note:
    //   Brancing kills perf.
    //   Defining unused vars after the if stmt is also bad.
    /* if (det == 0) { */
    /*     return vec3(-1,0,0); */
    /* } */
    /* return float(det == 0) * vec3(0) + */
    /*         (1 - float(det == 0)) * (1 / det) * vec3(dot(r, e2), */
    /*                                                 dot(q, s), */
    /*                                                 dot(r, ray.dir)); */
    return (1 / det) * vec3(dot(r, e2), dot(q,s), dot(r,ray.dir));
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

// Perf note: this approach is about 33% slower than rendering 6 arbitrary rects.
CollisionCube cube_collision(Cube c, Ray ray) {
    CollisionCube coll;  // Final coll
    CollisionFull _coll;  // Use it for each rect.
    coll.exists = false;
    Rect r;
    float min_t = 1 << 16;
// Front
    r.a = c.a;
    r.b = c.e;
    r.c = c.f;
    r.d = c.b;
    _coll = rect_collision(r, ray);

    if (_coll.exists && _coll.t < min_t) {
        min_t = _coll.t;
        coll.exists = true;
        coll.point = _coll.point;
        coll.normal = normal_for_rect(r);
        coll.t = _coll.t;
    }
// Left
    r.a = c.b;
    r.b = c.f;
    r.c = c.g;
    r.d = c.c;
    _coll = rect_collision(r, ray);

    if (_coll.exists && _coll.t < min_t) {
        min_t = _coll.t;
        coll.exists = true;
        coll.point = _coll.point;
        coll.normal = normal_for_rect(r);
        coll.t = _coll.t;
    }
// Back
    r.a = c.c;
    r.b = c.g;
    r.c = c.h;
    r.d = c.d;
    _coll = rect_collision(r, ray);

    if (_coll.exists && _coll.t < min_t) {
        min_t = _coll.t;
        coll.exists = true;
        coll.point = _coll.point;
        coll.normal = normal_for_rect(r);
        coll.t = _coll.t;
    }
// Right
    r.a = c.d;
    r.b = c.h;
    r.c = c.e;
    r.d = c.a;
    _coll = rect_collision(r, ray);

    if (_coll.exists && _coll.t < min_t) {
        min_t = _coll.t;
        coll.exists = true;
        coll.point = _coll.point;
        coll.normal = normal_for_rect(r);
        coll.t = _coll.t;
    }
// Top
    r.a = c.a;
    r.b = c.b;
    r.c = c.c;
    r.d = c.d;
    _coll = rect_collision(r, ray);

    if (_coll.exists && _coll.t < min_t) {
        min_t = _coll.t;
        coll.exists = true;
        coll.point = _coll.point;
        coll.normal = normal_for_rect(r);
        coll.t = _coll.t;
    }
// Bottom
    r.a = c.e;
    r.b = c.h;
    r.c = c.g;
    r.d = c.f;
    _coll = rect_collision(r, ray);

    if (_coll.exists && _coll.t < min_t) {
        min_t = _coll.t;
        coll.exists = true;
        coll.point = _coll.point;
        coll.normal = normal_for_rect(r);
        coll.t = _coll.t;
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
    float k1 = 320;
    float k2 = 1000.0;
    float k3 = 0.0;
    return k0 + r * (k1 + r * ( r * k2 + r * k3));
}

////////////////////////////////////////
// Primitive creation funcs.
////////////////////////////////////////
Cube MakeCube(vec3 center, vec3 size) {
    Cube c;
    c.center = center;
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
    c.a = c.center + rotate_vector_quat(c.a - c.center, q);
    c.b = c.center + rotate_vector_quat(c.b - c.center, q);
    c.c = c.center + rotate_vector_quat(c.c - c.center, q);
    c.d = c.center + rotate_vector_quat(c.d - c.center, q);
    c.e = c.center + rotate_vector_quat(c.e - c.center, q);
    c.f = c.center + rotate_vector_quat(c.f - c.center, q);
    c.g = c.center + rotate_vector_quat(c.g - c.center, q);
    c.h = c.center + rotate_vector_quat(c.h - c.center, q);
    return c;
}

// warp size: 64. (optimal warp size for my nvidia card)
layout(local_size_x = 8, local_size_y = 8) in;
void main() {
    //////
    // global
    //////

    Light l;
    l.position = vec3(0, -1, -2);
    l.color    = vec3(1,1,1);

    Rect f;
    float fw = 1.8;
    float fz = -1;
    f.a = vec3(-fw, -.5, -fw + fz);
    f.b = vec3(fw,  -.5, -fw + fz);
    f.c = vec3(fw,  -.5, fw + fz);
    f.d = vec3(-fw, -.5, fw + fz);

    Cube c = MakeCube(vec3(0,0,-2.0), vec3(0.1));
    c = rotate_cube(c, normalize(vec4(0,1,1, sphere_y*10)));

    Cube c2 = MakeCube(vec3(0, 0.5, -1.9 + sphere_y), vec3(0.05));

    Cube room = MakeCube(vec3(0,0,0), vec3(10));

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

    point *= barrel(radius_sq);

    point = rotate_vector_quat(point, orientation_q);

    // Camera movement
    eye.zx += camera_pos.xy;
    point.zx += camera_pos.xy;

    vec4 color;  // This ends up written to the image.

    if (occlude && radius_sq > 0.0016) {         // <--- Cull
        color = vec4(0);
    } else {                                     // <--- Ray trace.
        Ray ray;
        ray.o = point;
        ray.dir = ray.o - eye;

        float min_t = 1 << 16;    // Z-buffer substitute

        color = vec4(1, 1, 1, 1);

        CollisionCube cc;
        cc = cube_collision(c, ray);
/*j
        if (cc.exists && min_t > cc.t) {
            min_t = cc.t;
            color = vec4(lambert(cc.point, cc.normal, vec3(0.99,0.5,0.5), l), 1);
            Ray sr;
            sr.o = cc.point;
            sr.dir = l.position - sr.o;
            CollisionCube sc  = cube_collision(c2, sr);
            if (sc.exists) {
                color = mix(color, vec4(0), 0.5);
            }
        }

        cc = cube_collision(c2, ray);
        if (cc.exists && min_t > cc.t) {
            min_t = cc.t;
            color = vec4(lambert(cc.point, cc.normal, vec3(0.99,0.5,0.5), l), 1);
        }
*/

        cc = cube_collision(room, ray);
        if (cc.exists && min_t > cc.t) {
            min_t = cc.t;
            color = vec4(lambert(cc.point, -cc.normal, vec3(0.4, 0.4, 0.4), l), 1);
            Ray sr;
            sr.o = cc.point;
            sr.dir = l.position - sr.o;
            CollisionCube sc = cube_collision(c, sr);
            if (sc.exists && sc.t < 1) {
                color = mix(color, vec4(0), 0.5);
            }
            sc = cube_collision(c2, sr);
            if (sc.exists && sc.t < 1) {
                color = mix(color, vec4(0), 0.5);
            }
        }

// Transparent shit
        CollisionFull cr;
        cr = rect_collision(f, ray);
        if (cr.exists && cr.t < min_t) {
            min_t = cr.t;
            color = 0.5*vec4(lambert(cr.point, normal_for_rect(f), vec3(1), l), 0.5) + 0.5 * color;

            // Demo: Collide against rect.
            Ray sr;
            sr.o = cr.point;
            sr.dir = l.position - sr.o;
            CollisionCube sc = cube_collision(c, sr);
            if (sc.exists) {
                color = mix(color, vec4(0), 0.5);
            }
            sc = cube_collision(c2, sr);
            if (sc.exists) {
                color = mix(color, vec4(0), 0.5);
            }
        }
        min_t = 1 << 16;
        for (int i = 0; i < triangle_pool.data.length(); ++i) {
            Triangle t = triangle_pool.data[i];
            vec3 bar = barycentric(ray, t);
            if (bar.x > 0 &&
                    bar.y < 1 && bar.y > 0 &&
                    bar.z < 1 && bar.z > 0 &&
                    (bar.y + bar.z) < 1) {
                if (bar.x < min_t) {
                    min_t = bar.x;
                    color = vec4(bar.y*0, bar.z, 0, 1);
                    float u = bar.y;
                    float v = bar.z;
                    vec3 point = (1 - u - v) * t.p0 + u * t.p1 + v * t.p2;
                    color = vec4(lambert(point, -t.normal, vec3(1,1,1),l),1);
                }
            }
        }
        // TODO: Deal with possible negative min_t;
    }


    imageStore(tex, coord, color);
}

