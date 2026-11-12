#version 330 core

in vec3 v_worldPosition;
in vec3 v_viewPosition;
in vec3 v_worldNormal;
in vec3 v_viewNormal;
in vec2 v_texcoord;

out vec4 FragColor;

void main() {
    FragColor = vec4(1.0f);
}