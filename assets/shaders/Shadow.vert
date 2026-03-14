#version 450 core
layout(location = 0) in vec3 aPos;

uniform mat4 uLightVP;

void main() {
    gl_Position = uLightVP * vec4(aPos, 1.0);
}
