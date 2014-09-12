#version 430

layout(location = 1) uniform sampler2D fbo_texture;
layout(location = 2) uniform vec2 rcp_frame;

in vec2 coord;

out vec4 color;

vec4 FxaaPixelShader(
    vec2 pos,
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

void main() {
    vec4 aa_color = FxaaPixelShader(
        coord, //pos
        vec4(0), //fxaaConsolePosPos
        fbo_texture, //tex
        fbo_texture, //fxaaConsole360TexExpBiasNegOne
        fbo_texture, //fxaaConsole360TexExpBiasNegTwo
        rcp_frame,  //fxaaQualityRcpFrame  (1 / window_size)
        vec4(0), //fxaaConsoleRcpFrameOpt
        vec4(0), //fxaaConsoleRcpFrameOpt2
        vec4(0), //fxaaConsole360RcpFrameOpt2
        1.000, //fxaaQualitySubpix (default: 0.75)
        0.166, //fxaaQualityEdgeThreshold
        0.000, //fxaaQualityEdgeThresholdMin
        -1, //fxaaConsoleEdgeSharpness
        -1, //fxaaConsoleEdgeThreshold
        -1, //fxaaConsoleEdgeThresholdMin
        vec4(0)); //fxaaConsole360ConstDir

    //color.a = texture(fbo_texture, coord).a;
    color.rgb = aa_color.rgb;
    color.a = 1.0;
    /* color = texture(fbo_texture, coord); */
}

