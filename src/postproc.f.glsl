#version 430

layout(location = 1) uniform sampler2D fbo_texture;
layout(location = 2) uniform vec2 rcp_frame;
layout(location = 3) uniform vec2 screen_size;
layout(location = 4) uniform float lens_center_l;
layout(location = 5) uniform float lens_center_r;

in vec2 coord;

out vec4 color;

vec4 FxaaPixelShader(
    vec2 pos,
    vec2 pos_r,
    vec2 pos_b,
    vec4 fxaaConsolePosPos,
    sampler2D tex,
    sampler2D fxaaConsole360TexExpBiasNegOne,
    sampler2D fxaaConsole360TexExpBiasNegTwo,
    vec2 fxaaQualityRcpFrame,
    vec4 fxaaConsoleRcpFrameOpt,
    vec4 fxaaConsoleRcpFrameOpt2,
    vec4 fxaaConsole360RcpFrameOpt2,
    float fxaaQualitySubpix,
    float fxaaQualityEdgeThreshold,
    float fxaaQualityEdgeThresholdMin,
    float fxaaConsoleEdgeSharpness,
    float fxaaConsoleEdgeThreshold,
    float fxaaConsoleEdgeThresholdMin,
    vec4 fxaaConsole360ConstDir
    );

vec3 chroma(float rsq) {
    return vec3(rsq - 0.006, rsq, rsq + 0.014);
}

void main() {

    vec2 point = coord;  // in [0,1]^2 * screen_size
    int eye = 0;
    if (point.x < 0.5) {
        eye = -1;
    } else {
        point.x -= 0.5;
        eye = 1;
    }
    /* point.x /= 2;  // One viewport per eye */
    point.xy *= screen_size;

    if (eye == -1) {
        point -= lens_center_l;
    } else {
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

    // point *= vec2(rsq);
    /* point -= vec2(0.25); */

    point.xy += vec2(0.5); // Back to 0,0
    point.x /= 2; // [0, 1] -> [0, 0.5]

    point_r.xy += vec2(0.5); // Back to 0,0
    point_r.x /= 2; // [0, 1] -> [0, 0.5]

    point_b.xy += vec2(0.5); // Back to 0,0
    point_b.x /= 2; // [0, 1] -> [0, 0.5]

    if (eye == 1) {
        point.x += 0.5;
        point_r.x += 0.5;
        point_b.x += 0.5;
    }

    vec3 scale = chroma(rsq);
    vec2 coord_r = point_r;
    vec2 coord_g = coord;
    vec2 coord_b = point_b;

// NOTE: Using 0.063 ("overkill") as edge threshold because we may be downsampling..
    vec4 aa_color = FxaaPixelShader(
        coord_g,
        coord_r,
        coord_b,
        vec4(0), //fxaaConsolePosPos
        fbo_texture, //tex
        fbo_texture, //fxaaConsole360TexExpBiasNegOne
        fbo_texture, //fxaaConsole360TexExpBiasNegTwo
        rcp_frame,  //fxaaQualityRcpFrame  (1 / window_size)
        vec4(0), //fxaaConsoleRcpFrameOpt
        vec4(0), //fxaaConsoleRcpFrameOpt2
        vec4(0), //fxaaConsole360RcpFrameOpt2
        0.000, //fxaaQualitySubpix (default: 0.75)
        0.063, //fxaaQualityEdgeThreshold
        0.0312, //fxaaQualityEdgeThresholdMin
        -1, //fxaaConsoleEdgeSharpness
        -1, //fxaaConsoleEdgeThreshold
        -1, //fxaaConsoleEdgeThresholdMin
        vec4(0)); //fxaaConsole360ConstDir

    //color.a = texture(fbo_texture, coord).a;
    color.rgb = aa_color.rgb;
    color.a = 1.0;
    /* color = texture(fbo_texture, coord); */
}

