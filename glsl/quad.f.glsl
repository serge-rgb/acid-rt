#version 430

layout(location=1)uniform sampler2D tex;

in vec2 coord;

out vec4 color;

void main() {
    color = texture(tex, coord);
}
