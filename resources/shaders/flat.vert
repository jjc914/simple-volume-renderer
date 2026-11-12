#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_worldPosition;
out vec3 v_viewPosition;
out vec3 v_worldNormal;
out vec3 v_viewNormal;
out vec2 v_texcoord;

void main() {
    mat4 mvp = u_proj * u_view * u_model;
    vec4 worldPosition = u_model * vec4(a_position, 1.0f);
    vec4 worldNormal = u_model * vec4(a_normal, 0.0f);

    gl_Position = mvp * vec4(a_position, 1.0f);

    v_worldPosition = worldPosition.xyz;
    v_viewPosition = (u_view * worldPosition).xyz;
    v_worldNormal = worldNormal.xyz;
    v_viewNormal = (u_view * worldNormal).xyz;
    v_texcoord = a_texcoord;
}