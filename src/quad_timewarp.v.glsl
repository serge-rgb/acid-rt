#version 430

layout(location = 0) in vec2 position;

layout(location = 10) uniform mat4 rotation;
layout(location = 11) uniform bool tw_enabled;

out vec2 coord;

void main()
{
    if (tw_enabled)
    {
        float f = 1.0;
        // direct to clip space. must be in [-1, 1]^2
        vec4 rotated = rotation * vec4(f * position, 1.00, 0.0);
        vec2 projected = vec2(rotated.x / rotated.z, rotated.y / rotated.z);
        gl_Position = vec4((1/f) * projected, 0, 1);
        coord = ((1/f) * projected + vec2(1,1))/2;
    }
    else
    {
        coord = (position + vec2(1,1))/2;
        gl_Position = vec4(position, 0, 1);
    }
}

