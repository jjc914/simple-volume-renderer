#version 330 core

in vec2 v_ndc;

uniform mat4 u_invViewProj;
uniform vec3 u_cameraPos;
uniform vec3 u_boundsMin;
uniform vec3 u_boundsMax;

uniform sampler3D u_volume;
uniform sampler1D u_transfer;

uniform int   u_maxSteps;
uniform float u_stepSize;       // in volume-local [0,1]^3 units
uniform float u_densityScale;
uniform float u_opacityCutoff;

out vec4 FragColor;   // premultiplied alpha

void main() {
    // World-space ray through this pixel.
    vec4 far = u_invViewProj * vec4(v_ndc, 1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w - u_cameraPos);

    // Ray / AABB slab intersection.
    vec3 inv = 1.0 / dir;
    vec3 t0 = (u_boundsMin - u_cameraPos) * inv;
    vec3 t1 = (u_boundsMax - u_cameraPos) * inv;
    vec3 tmin = min(t0, t1), tmax = max(t0, t1);
    float tNear = max(max(tmin.x, tmin.y), max(tmin.z, 0.0));
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    if (tFar <= tNear) discard;

    // March in volume-local space so u_stepSize is resolution-independent.
    vec3 extent = u_boundsMax - u_boundsMin;
    vec3 pos = (u_cameraPos + dir * tNear - u_boundsMin) / extent;
    vec3 step = dir / extent;
    step *= u_stepSize / length(step);
    float stepWorld = length(step * extent);

    vec3 color = vec3(0.0);
    float alpha = 0.0;
    float travelled = tNear;

    for (int i = 0; i < u_maxSteps && travelled < tFar; ++i) {
        float s = texture(u_volume, pos).r;
        vec4 tf = texture(u_transfer, s);

        // Opacity correction so the result doesn't depend on step size.
        float a = 1.0 - pow(1.0 - clamp(tf.a * u_densityScale, 0.0, 0.999), u_stepSize * 100.0);
        color += (1.0 - alpha) * a * tf.rgb;
        alpha += (1.0 - alpha) * a;

        if (alpha >= u_opacityCutoff) break;
        pos += step;
        travelled += stepWorld;
    }

    FragColor = vec4(color, alpha);
}
