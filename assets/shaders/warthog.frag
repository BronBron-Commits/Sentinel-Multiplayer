#version 120

varying vec3 v_normal;
varying vec3 v_world;

uniform vec3 u_light_dir;
uniform vec3 u_color;
uniform float u_roughness;

void main()
{
    vec3 N = normalize(v_normal);
    vec3 L = normalize(-u_light_dir);

    float ndl = max(dot(N, L), 0.0);

    vec3 diffuse = u_color * ndl;
    vec3 ambient = u_color * 0.25;

    gl_FragColor = vec4(diffuse + ambient, 1.0);
}
