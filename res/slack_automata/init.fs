#version 460

// Lumpy-noise initialization from Slackermanz/VulkanAutomata.
layout(location = 0) out vec4 out_col;
uniform uint rand_seed;

//! option "Seed" (0, 255)
uniform uint seed = 93u;
//! option "R Power" (0, 1)
uniform float power_r = 1.0;
//! option "G Power" (0, 1)
uniform float power_g = 1.0;
//! option "B Power" (0, 1)
uniform float power_b = 1.0;
//! option "R Scale" exp (0.05, 10)
uniform float scale_r = 1.0;
//! option "G Scale" exp (0.05, 10)
uniform float scale_g = 1.0;
//! option "B Scale" exp (0.05, 10)
uniform float scale_b = 1.0;
//! option "R Amplitude" (0, 1)
uniform float amp_r = 0.4;
//! option "G Amplitude" (0, 1)
uniform float amp_g = 0.4;
//! option "B Amplitude" (0, 1)
uniform float amp_b = 0.4;

float coordinate_noise(float x, float y, float modulus) {
    float root = sqrt(mod(x * y + y, modulus)) / sqrt(modulus);
    float squared = mod((x * x) + (y * y), modulus) / modulus;
    return clamp((root + squared) * 0.5, 0.0, 1.0);
}

float scramble_noise(float x, float y, float modulus, float value) {
    value = value * mod(x * y + x, modulus);
    return value - floor(value);
}

float offset_noise(float x, float y, float modulus0, float modulus1, float dx, float dy) {
    float value = coordinate_noise(x + dx, y + dy, modulus0);
    return scramble_noise(x + dx, y + dy, modulus1, value);
}

float lump(float x, float y, float radius, float modulus0, float modulus1) {
    float count = 0.0;
    float sum = 0.0;
    for (float dx = -radius; dx <= radius; ++dx) {
        for (float dy = -radius; dy <= radius; ++dy) {
            if (round(sqrt(dx * dx + dy * dy)) <= radius) {
                sum += offset_noise(x, y, modulus0, modulus1, dx, dy);
                count += 1.0;
            }
        }
    }
    float average = sum / count;
    float value = average;
    for (float i = 0.0; i <= radius; ++i)
        value = clamp((average * value + average * value) * (average + average), 0.0, 1.0);
    return value;
}

float reseed(uint channel_seed, float noise_scale, float amplitude) {
    float x = gl_FragCoord.x;
    float y = gl_FragCoord.y;
    float value = float(channel_seed);
    float r0 = lump(x, y, round(6.0 * noise_scale),
                    19.0 + mod(value, 17.0), 23.0 + mod(value, 43.0));
    float r1 = lump(x, y, round(22.0 * noise_scale),
                    13.0 + mod(value, 29.0), 17.0 + mod(value, 31.0));
    float r2 = lump(x, y, round(14.0 * noise_scale),
                    13.0 + mod(value, 11.0), 51.0 + mod(value, 37.0));
    float r3 = lump(x, y, round(18.0 * noise_scale),
                    29.0 + mod(value, 7.0), 61.0 + mod(value, 28.0));
    return clamp(sqrt((r0 + r1) * r3 * (amplitude + 1.2)) - r2 * (amplitude * 1.8 + 0.2), 0.0, 1.0);
}

void main() {
    uint initial_seed = seed + rand_seed;
    out_col = vec4(
        power_r * reseed(initial_seed, scale_r, amp_r),
        power_g * reseed(initial_seed + 1u, scale_g, amp_g),
        power_b * reseed(initial_seed + 2u, scale_b, amp_b),
        1.0);
}
