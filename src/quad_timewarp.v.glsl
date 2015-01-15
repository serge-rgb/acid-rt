#version 430

layout(location = 0) in vec2 position;

layout(location = 10) uniform mat4 rotation;

out vec2 coord;

void main()
{
    coord = (position + vec2(1,1))/2;
    // direct to clip space. must be in [-1, 1]^2
    vec4 rotated = rotation * vec4(position, 1.0, 0.0);
    vec2 projected = vec2(rotated.x / rotated.z, rotated.y / rotated.z);
    gl_Position = vec4(projected, 0, 1);
}

