#version 460

// Based on Slackermanz/VulkanAutomata. Initialization and painting are separate passes.
layout(location = 0) out vec4 out_col;
uniform sampler2DArray tex;
uniform uint[12] nb;
uniform uint[24] ur;
uniform uint[2] us;
uniform uint[3] ch;
uniform uint[3] ch2;
uniform uint[3] ch3;
uniform uint mode;
uniform float zoom;
uniform float scale;

//! option "Max radius" (2, 16)
const uint MAX_RADIUS = 12u;

const float PI = 3.14159265359;
const float E = 2.71828182846;
const float PRECISION = 65536.0;

struct Neighborhood {
    vec4 sum;
    float weight;
};

uint unpack_bits(uint value, uint bits, uint offset) {
    return (value >> offset) & ((1u << bits) - 1u);
}

vec4 sample_cell(ivec2 offset) {
    ivec2 size = textureSize(tex, 0).xy;
    ivec2 position = ivec2(gl_FragCoord.xy) + offset;
    position = (position % size + size) % size;
    return texelFetch(tex, ivec3(position, 0), 0);
}

Neighborhood read_ring(vec2 radius) {
    Neighborhood result = Neighborhood(vec4(0.0), 0.0);
    vec2 squared_radius = ceil(radius + vec2(0.5)) - vec2(0.500001);
    squared_radius *= squared_radius;
    for (float y = 0.0; y <= radius.x; ++y) {
        vec2 bounds = sqrt(max(vec2(0.0), squared_radius - vec2(y * y)));
        for (float x = floor(bounds.y) + 1.0; x <= bounds.x; ++x) {
            result.weight += PRECISION * 4.0;
            result.sum += floor(sample_cell(ivec2(x, y)) * PRECISION);
            result.sum += floor(sample_cell(ivec2(y, -x)) * PRECISION);
            result.sum += floor(sample_cell(ivec2(-x, -y)) * PRECISION);
            result.sum += floor(sample_cell(ivec2(-y, x)) * PRECISION);
        }
    }
    return result;
}

Neighborhood select_rings(Neighborhood[MAX_RADIUS] rings, uint mask, uint offset) {
    Neighborhood result = Neighborhood(vec4(0.0), 0.0);
    for (uint ring = 0u; ring < MAX_RADIUS; ++ring) {
        if (unpack_bits(mask, 1u, ring + offset) != 0u) {
            result.sum += rings[ring].sum;
            result.weight += rings[ring].weight;
        }
    }
    return result;
}

float parameter_scale() {
    if (mode == 1u) {
        float position = gl_FragCoord.x / textureSize(tex, 0).x;
        return ((position + zoom) * (scale / (1.0 + zoom * 2.0))) * 2.0;
    }
    if (mode == 2u) {
        vec2 size = vec2(textureSize(tex, 0).xy);
        vec2 position = ((gl_FragCoord.xy - size * 0.5) / size) * 0.5;
        float distance = sqrt(position.x * position.x + position.y * position.y);
        return ((sqrt(distance) + zoom) * (scale / (1.0 + zoom * 2.0))) * 2.0;
    }
    return scale;
}

float threshold(uint rule, uint byte, float rule_scale) {
    uint value = unpack_bits(rule, 8u, byte * 8u);
    return (float(value + 1u) / 256.0) * ((rule_scale * 0.5) / 128.0);
}

float update_sign(uint signs, uint offset) {
    return float(unpack_bits(signs, 1u, offset) * 2u) - 1.0;
}

vec4 sigmoid(vec4 value, float width) {
    return 1.0 / (1.0 + exp((-width * 2.0 * value * (PI / 2.0)) + width * (PI / 2.0)));
}

void main() {
    Neighborhood[MAX_RADIUS] rings;
    for (uint ring = 0u; ring < MAX_RADIUS; ++ring)
        rings[ring] = read_ring(vec2(ring + 1u, ring));

    vec4 current = sample_cell(ivec2(0));
    vec4 next = current;
    float rule_scale = parameter_scale();
    const float step_size = (1.0 / PRECISION) * 48.0 * 64.0;
    const float decay_size = (1.0 / PRECISION) * 48.0 * 2.0;

    for (uint rule = 0u; rule < 24u; ++rule) {
        Neighborhood neighborhood = select_rings(rings, nb[rule / 2u], (rule & 1u) * 16u);
        // An empty mask disables the rule instead of producing 0/0.
        if (neighborhood.weight == 0.0)
            continue;

        uint offset = (rule * 4u) & 31u;
        uint input_channel = unpack_bits(ch[rule / 8u], 2u, offset);
        if (input_channel == 3u)
            input_channel = unpack_bits(ch2[rule / 8u], 2u, offset);
        uint output_channel = unpack_bits(ch[rule / 8u], 2u, offset + 2u);
        if (output_channel == 3u)
            output_channel = unpack_bits(ch2[rule / 8u], 2u, offset + 2u);
        uint modulation_channel = unpack_bits(ch3[rule / 8u], 2u, offset + 2u);
        if (modulation_channel == 3u)
            modulation_channel = unpack_bits(ch[rule / 8u], 2u, offset + 2u);

        float value = sigmoid(neighborhood.sum / neighborhood.weight, E)[input_channel];
        float modulation = current[modulation_channel];
        float amplitude = 3.0 * ((modulation - 0.5) * (modulation - 0.5)) + 0.25;
        if (value >= threshold(ur[rule], 0u, rule_scale) &&
            value <= threshold(ur[rule], 1u, rule_scale)) {
            next[output_channel] += update_sign(us[rule / 16u], (rule * 2u) & 31u) * step_size * amplitude;
        }
        if (value >= threshold(ur[rule], 2u, rule_scale) &&
            value <= threshold(ur[rule], 3u, rule_scale)) {
            next[output_channel] += update_sign(us[rule / 16u], (rule * 2u + 1u) & 31u) * step_size * amplitude;
        }
    }

    next -= sigmoid(next, 0.5) * decay_size * 64.0 + decay_size;
    next.a = 1.0;
    out_col = clamp(next, 0.0, 1.0);
}
