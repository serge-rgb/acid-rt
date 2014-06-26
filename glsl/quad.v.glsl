#version 430

in vec2 position;

void main() {
    // direct to clip space. must be
    gl_Position = vec4(position.x, position.y, 0, 1);
}
