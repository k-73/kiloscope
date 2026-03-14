#version 450 core
uniform uint uPickId;

layout(location = 0) out uint FragId;

void main() {
    FragId = uPickId;
}
