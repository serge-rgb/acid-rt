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
float EPSILON = 0.00001;


// Sync this with enum in C codebase.
int MaterialType_Lambert = 0;

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

struct Primitive {
    int offset;  // Into triangle_pool
    int num_triangles;
    int material;
};

struct AABB {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
    float zmin;
    float zmax;
};

struct BVHNode {
    int primitive_offset;
    int r_child_offset;
    AABB bbox;
};

struct Light {
    vec3 position;
    vec3 color;
};

layout(std430, binding = 0) buffer TrianglePool {
    Triangle data[];
} triangle_pool;

layout(std430, binding = 1) buffer LightPool {
    Light data[];
} light_pool;

layout(std430, binding = 2) buffer PrimitivePool {
    Primitive data[];
} primitive_pool;

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
};

vec3 normal_for_rect(Rect r) {
    return normalize(cross(r.a - r.b, r.c - r.b));
}

////////////////////////////////////////
// Collision structs
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

float bbox_collision(AABB box, Ray ray) {
    float t0 = 0;
    float t1 = 1 << 16;
    float xmin, xmax, ymin, ymax, zmin, zmax;

    xmin = (box.xmin - ray.o.x) / ray.dir.x;
    xmax = (box.xmax - ray.o.x) / ray.dir.x;
    if (xmin > xmax) {
        float tmp = xmin;
        xmin = xmax;
        xmax = tmp;
    }

    ymin = (box.ymin - ray.o.y) / ray.dir.y;
    ymax = (box.ymax - ray.o.y) / ray.dir.y;
    if (ymin > ymax) {
        float tmp = ymin;
        ymin = ymax;
        ymax = tmp;
    }

    zmin = (box.zmin - ray.o.z) / ray.dir.z;
    zmax = (box.zmax - ray.o.z) / ray.dir.z;
    if (zmin > zmax) {
        float tmp = zmin;
        zmin = zmax;
        zmax = tmp;
    }

    t0 = max(max(xmin, ymin), zmin);
    t1 = min(min(xmax, ymax), zmax);

    if (t0 < t1) return t0 < 0? t1 : t0;

    return -1 << 16;
}

vec3 barycentric(Ray ray, Triangle tri) {
    vec3 e1 = tri.p1 - tri.p0;
    vec3 e2 = tri.p2 - tri.p0;
    vec3 s  = ray.o - tri.p0;
    vec3 m  = cross(s, ray.dir);
    vec3 n = cross(e1, e2);
    float det = dot(-n, ray.dir);
    //if (det <= EPSILON && det >= -EPSILON) return vec3(-1);
    return (1 / det) * vec3(dot(n, s), dot(m, e2), dot(-m, e1));
}


// ========================================
// Material functions.
// ========================================

vec3 lambert(vec3 point, vec3 normal, vec3 color, Light l) {
    //return normal;
    float d = max(dot(normal, normalize(l.position - point)), 0);
    return color * d;
}

float barrel(float r) {
    float k0 = 1.0;
    float k1 = 300;
    float k2 = 400.0;
    float k3 = 0.0;
    return k0 + r * (k1 + r * ( r * k2 + r * k3));
}

// (x * y) % 32 == 0
layout(local_size_x = 16, local_size_y = 8) in;
void main() {
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

        // Single trace against triangle pool
        float min_t = 1 << 16;
        color = vec4(0, 0, 0, 1);
        vec3 point;
        vec3 normal;
        vec2 uv;

/*         for (int i = 0; i < primitive_pool.data.length(); ++i) { */
/*             Primitive p = primitive_pool.data[i]; */
/*             for (int j = p.offset; j < p.offset + p.num_triangles; ++j) { */
/*                 Triangle t = triangle_pool.data[j]; */
/*                 vec3 bar = barycentric(ray, t); */
/*                 if (bar.x > 0 && */
/*                         bar.y < 1 && bar.y > 0 && */
/*                         bar.z < 1 && bar.z > 0 && */
/*                         (bar.y + bar.z) < 1) { */
/*                     if (bar.x < min_t) { */
/*                         min_t = bar.x; */
/*                         float u = bar.y; */
/*                         float v = bar.z; */
/*                         point = ray.o + bar.x * ray.dir; */
/*                         //point = (1 - u - v) * t.p0 + u * t.p1 + v * t.p2; */
/*                         normal = t.normal; */
/*                         uv = vec2(u,v); */
/*                     } */
/*                 } */
/*             } */
/*         } */
        // --- actual trace ends here

        AABB box;
        box.xmin = -2;
        box.xmax = 2;
        box.ymin = -2;
        box.ymax = 2;
        box.zmin = -6;
        box.zmax = -2;
        float box_t;
        for (int as = 0; as < 300; as++)
        {
            box_t = bbox_collision(box, ray);
        }
        if (box_t > 0) {
            min_t = box_t;
            color = vec4(1);
        }

        if (min_t < 1 << 16) {
            int num_lights = light_pool.data.length();
            for (int i = 0; i < num_lights; ++i) {
                Light light = light_pool.data[i];
                /* light.position = vec3(0,100,0); */
                vec3 rgb = (1.0 / num_lights) * lambert(point, normal, vec3(1), light);
                /* vec3 rgb = point; */
                color += vec4(rgb, 1);
            }

        }
        // TODO: Deal with possible negative min_t;
    }


    imageStore(tex, coord, color);
}

