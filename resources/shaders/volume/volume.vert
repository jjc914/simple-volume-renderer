#version 330 core

out vec2 v_ndc;

// Fullscreen triangle from gl_VertexID; no vertex buffers needed.
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    v_ndc = p * 2.0 - 1.0;
    gl_Position = vec4(v_ndc, 0.0, 1.0);
}
