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

vec3 barycentric(Ray ray, vec3 p0, vec3 p1, vec3 p2) {
    vec3 e1 = p1 - p0;
    vec3 e2 = p2 - p0;
    vec3 s  = ray.o - p0;
    vec3 d = ray.dir;
    vec3 q = cross(d, e2);
    vec3 r = cross(s, e1);
    float det = dot(q, e1);
    if (det >= -EPSILON && det <= EPSILON) {
        return vec3(-1, 0, 0);
    }
    return (1 / det) * vec3(dot(r, e2),
                            dot(q, s),
                            dot(r, d));
}

struct Plane {
    vec3 normal;
    vec3 point;
};

struct Rect {
    vec3 a,b,c,d;
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

Collision plane_collision_p(Plane p, Ray r) {
    Collision coll;
    coll.exists = false;

    float disc = dot(r.dir, p.normal);
    if (disc == 0) {
        return coll;
    }
    coll.exists = true;
    return coll;
}

CollisionFull plane_collision(Plane p, Ray r) {
    CollisionFull coll;
    coll.exists = false;
    coll.t = -2;
    float disc = dot(r.dir, p.normal);
    if (disc == 0) {
        return coll;
    }

    coll.exists = true;
    float t = dot((p.point - r.o), p.normal);
    coll.t = t;
    if (t < 0) return coll;

    coll.exists = true;
    coll.point = r.o + (t * r.dir);

    return coll;
}

CollisionFull rect_collision(Rect rect, Ray r) {
    vec3 normal = normalize(cross(rect.c - rect.b, rect.d - rect.b));
    Plane plane;
    plane.normal = normal;
    plane.point = rect.a;
    CollisionFull coll = plane_collision(plane, r);

    if (!coll.exists) {
        return coll;
    }
    coll.exists = false;

    vec3 b_coord_0 = barycentric(r, rect.a, rect.b, rect.c);
    vec3 b_coord_1 = barycentric(r, rect.a, rect.c, rect.d);

    for (int i = 0; i < 2; i++)
    {
        vec3 b_coord;
        if (i == 0) {
            b_coord = b_coord_0;
        } else {
            b_coord = b_coord_1;
        }
        float z = 1 - b_coord.y - b_coord.z;
        if (b_coord.x > 0 &&
            b_coord.y <= 1.0 && b_coord.y >= 0 &&
            b_coord.z <= 1.0 && b_coord.z >= 0 &&
            z <= 1 && z >= 0)
        {
            coll.exists = true;
            coll.t = b_coord.x;
        }

    }

    return coll;
}

// ========================================
// Material functions.
// ========================================

// Light should not be a parameter. All lights should contribute. -- Right?
vec3 lambert(vec3 point, vec3 normal, vec3 color, Light l) {
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
    s.center = vec3(0, sphere_y, -0.5);

    Plane p;
    p.normal = vec3(0, 1, 0);
    p.point  = vec3(0, -100, 0);

    Rect r;
    float w = 100;
    float h = 100;
    float z = -20;
    r.a = vec3(-w, -h, z);
    r.b = vec3(w , -h, z);
    r.c = vec3(w , h , z);
    r.d = vec3(-w, h , z);

    Rect floor;
    float fw = 10;
    floor.a = vec3(-fw, -1, -fw);
    floor.b = vec3(fw,  -1, -fw);
    floor.c = vec3(fw,  -1, fw);
    floor.d = vec3(-fw, -1, fw);

    Light l;
    l.position = vec3(0, 3, 3);
    l.color    = vec3(1,1,1);

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

        color = vec4(0.1, 0.1, 0.5, 1);
        CollisionFull cf = sphere_collision(s, ray);
        if (cf.exists && min_t > cf.t) {
            min_t = cf.t;
            color = vec4(lambert(cf.point, normalize(cf.point - s.center), vec3(0,0,1), l), 1);
        }

        CollisionFull cp = rect_collision(r, ray);
        if (cp.exists && min_t > cp.t) {
            min_t = cp.t;
            color = vec4(1);
            color = vec4(1,0,0,1);
            color = vec4(lambert(cp.point, normalize(cross(r.c - r.b, r.d - r.b)), vec3(0,1,0), l), 1);
        }
        cp = rect_collision(floor, ray);
        if (cp.exists && min_t > cp.t) {
            min_t = cp.t;
            color = vec4(1);
            color = vec4(1,0,0,1);
            color = vec4(lambert(cp.point, normalize(cross(r.c - r.b, r.d - r.b)), vec3(0.5,0.5,0.5), l), 1);
        }
        // TODO: Deal with possible negative min_t;
    }

    imageStore(tex, coord, color);
}

