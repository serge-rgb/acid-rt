#version 430

layout(location = 1) uniform sampler2D fbo_texture;
layout(location = 2) uniform vec2 rcp_frame;
layout(location = 3) uniform vec2 screen_size;
layout(location = 4) uniform float lens_center_l;
layout(location = 5) uniform float lens_center_r;

in vec2 coord;

out vec4 color;

vec3 chroma(float rsq)
{
    return vec3(rsq - 0.006, rsq, rsq + 0.014);
}

void main()
{

    vec2 point = coord;  // in [0,1]^2 * screen_size

    // perf: this forces "dynamic" branches that would be static if we just used `coord`
    int eye = 0;
    if (point.x < 0.5)
    {
        eye = -1;
    }
    else
    {
        point.x -= 0.5;
        eye = 1;
    }
    /* point.x /= 2;  // One viewport per eye */
    point.xy *= screen_size;

    if (eye == -1)
    {
        point -= lens_center_l;
    }
    else
    {
        point -= lens_center_r;
    }

    // Back to unit
    point.xy /= screen_size;

    point.x *= 2; // [0, 0.5] -> [0, 1]
    point.xy -= vec2(0.5); // Center.

    bool below = coord.y < 0.5;

    float rsq = point.x * point.x + point.y * point.y;

    float r = sqrt(rsq);
    float theta = acos(point.x / r);
    if (below) theta = 2 * (3.14159) - theta;

    /* float chroma_threshold = 0.25; */
    float r_factor = 0.975;
    /* r_factor = float(rsq < chroma_threshold) * r_factor + float(rsq >= chroma_threshold) * 1.0; */
    float b_factor = 1.025;
    /* b_factor = float(rsq < chroma_threshold) * b_factor + float(rsq >= chroma_threshold) * 1.0; */
    vec2 point_r = r * sqrt(r_factor) * vec2(cos(theta), sin(theta));
    //vec2 point_r = r * sqrt(1.0) * vec2(cos(theta), sin(theta));
    vec2 point_b = r * sqrt(b_factor) * vec2(cos(theta), sin(theta));

    point.xy += vec2(0.5); // Back to 0,0
    point.x /= 2; // [0, 1] -> [0, 0.5]

    point_r.xy += vec2(0.5); // Back to 0,0
    point_r.x /= 2; // [0, 1] -> [0, 0.5]

    point_b.xy += vec2(0.5); // Back to 0,0
    point_b.x /= 2; // [0, 1] -> [0, 0.5]

    if (eye == 1)
    {
        point.x += 0.5;
        point_r.x += 0.5;
        point_b.x += 0.5;
    }

    vec3 scale = chroma(rsq);

    vec2 coord_r = point_r;
    vec2 coord_g = coord;
    vec2 coord_b = point_b;

    color.r = texture(fbo_texture, coord_r).r;
    color.g = texture(fbo_texture, coord_g).g;
    color.b = texture(fbo_texture, coord_b).b;

    color.a = 1.0;
}

