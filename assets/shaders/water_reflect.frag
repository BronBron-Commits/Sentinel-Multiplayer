in vec3 vNormal;

#version 330 core

// Simple 2D noise for surface distortion
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));

    p += dot(p, p + 45.32);

    return fract(p.x * p.y);
}
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);

    // Add subtle normal distortion for water surface
    // Scroll the noise for a flowing effect
    float flowSpeed = 2.0;
    vec2 flow = vec2(0.0, uTime * flowSpeed);
    float n = noise(vWorldPos.xz * 2.5 + flow);
    N = normalize(mix(N, vec3(0.0, 1.0, 0.0), 0.15 * n));

    // Reflection vector
    vec3 R = reflect(-V, N);
    float skyAmount = clamp(R.y, 0.0, 1.0);
    vec3 waterBase = vec3(0.18, 0.55, 0.95); // blue path color

    // Strong fresnel for sharp edge reflections
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0); // Lower power for stronger fresnel
    float reflectStrength = mix(max(uReflectivity, 0.7), 1.0, fresnel * 2.0); // Higher base reflectivity, stronger fresnel

    // Much sharper and brighter highlight (simulate sun)
    float sun = pow(clamp(dot(R, vec3(0.4, 1.0, 0.3)), 0.0, 1.0), 256.0);
    vec3 sunColor = vec3(1.0, 1.0, 0.95) * sun * 2.0;

    // Final color: blend water, sky, and sun highlight
    vec3 color = mix(waterBase, uSkyColor, skyAmount * reflectStrength);
    color += sunColor;

    // --- Add sparkles ---
    float sparkleDensity = 0.18; // Lower = more sparkles
    float sparkleSize = 0.12;    // Controls sharpness
    float sparkleSpeed = 1.5;
    float sparkle = hash(floor(vWorldPos.xz * 12.0) + uTime * sparkleSpeed);
    float sparkleMask = smoothstep(1.0 - sparkleSize, 1.0, sparkle) * step(fract(uTime * 0.7 + vWorldPos.x * 0.3 + vWorldPos.z * 0.5), sparkleDensity);
    color += vec3(2.5, 2.3, 1.8) * sparkleMask;

    // Blend with original terrain color if not path
    vec3 terrainColor = vec3(0.42, 0.36, 0.28); // fallback terrain color
    color = mix(terrainColor, color, uIsPath);

    FragColor = vec4(color, 1.0);
}
