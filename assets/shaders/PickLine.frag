// Line pick fragment shader — writes per-vertex pick ID to R32UI target.
#version 450 core

flat in uint vPickId;

layout(location = 0) out uint FragId;

void main() {
    FragId = vPickId;
}
