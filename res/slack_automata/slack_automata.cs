#version 460

// Compute counterpart of slack_automata.fs; keep rule arithmetic and sampling order identical.
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(rgba32f, binding = 0) readonly uniform image2DArray tex;
layout(rgba32f, binding = 2) writeonly uniform image2DArray output_image;
// Base layer for partial array dispatches; each layer uses the matching rule.
uniform int layer = 0;

struct SlackRule {
    uint nb[12];
    uint ur[24];
    uint us[2];
    uint ch[3];
    uint ch2[3];
    uint ch3[3];
    uint mode;
    float zoom;
    float scale;
};
layout(std430, binding = 4) readonly buffer SlackRules {
    SlackRule rules[];
};

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
    ivec2 size = imageSize(tex).xy;
    ivec2 position = ivec2(gl_GlobalInvocationID.xy) + offset;
    position = (position % size + size) % size;
    return imageLoad(tex, ivec3(position, int(gl_GlobalInvocationID.z) + layer));
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

float parameter_scale(uint mode, float zoom, float scale) {
    // Fragment shaders evaluate gl_FragCoord at pixel centers, not integer coordinates.
    vec2 pixel_center = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
    if (mode == 1u) {
        float position = pixel_center.x / imageSize(tex).x;
        return ((position + zoom) * (scale / (1.0 + zoom * 2.0))) * 2.0;
    }
    if (mode == 2u) {
        vec2 size = vec2(imageSize(tex).xy);
        vec2 position = ((pixel_center - size * 0.5) / size) * 0.5;
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
    ivec3 coord = ivec3(gl_GlobalInvocationID) + ivec3(0, 0, layer);
    if (any(lessThan(coord, ivec3(0))) ||
        any(greaterThanEqual(coord, imageSize(tex))) ||
        any(greaterThanEqual(coord, imageSize(output_image))) ||
        coord.z >= rules.length())
        return;

    Neighborhood[MAX_RADIUS] rings;
    for (uint ring = 0u; ring < MAX_RADIUS; ++ring)
        rings[ring] = read_ring(vec2(ring + 1u, ring));

    vec4 current = sample_cell(ivec2(0));
    vec4 next = current;
    float rule_scale = parameter_scale(rules[coord.z].mode, rules[coord.z].zoom, rules[coord.z].scale);
    const float step_size = (1.0 / PRECISION) * 48.0 * 64.0;
    const float decay_size = (1.0 / PRECISION) * 48.0 * 2.0;

    for (uint rule = 0u; rule < 24u; ++rule) {
        Neighborhood neighborhood = select_rings(rings, rules[coord.z].nb[rule / 2u], (rule & 1u) * 16u);
        // An empty mask disables the rule instead of producing 0/0.
        if (neighborhood.weight == 0.0)
            continue;

        uint offset = (rule * 4u) & 31u;
        uint input_channel = unpack_bits(rules[coord.z].ch[rule / 8u], 2u, offset);
        if (input_channel == 3u)
            input_channel = unpack_bits(rules[coord.z].ch2[rule / 8u], 2u, offset);
        uint output_channel = unpack_bits(rules[coord.z].ch[rule / 8u], 2u, offset + 2u);
        if (output_channel == 3u)
            output_channel = unpack_bits(rules[coord.z].ch2[rule / 8u], 2u, offset + 2u);
        uint modulation_channel = unpack_bits(rules[coord.z].ch3[rule / 8u], 2u, offset + 2u);
        if (modulation_channel == 3u)
            modulation_channel = unpack_bits(rules[coord.z].ch[rule / 8u], 2u, offset + 2u);

        float value = sigmoid(neighborhood.sum / neighborhood.weight, E)[input_channel];
        float modulation = current[modulation_channel];
        float amplitude = 3.0 * ((modulation - 0.5) * (modulation - 0.5)) + 0.25;
        if (value >= threshold(rules[coord.z].ur[rule], 0u, rule_scale) &&
            value <= threshold(rules[coord.z].ur[rule], 1u, rule_scale)) {
            next[output_channel] += update_sign(rules[coord.z].us[rule / 16u], (rule * 2u) & 31u) * step_size * amplitude;
        }
        if (value >= threshold(rules[coord.z].ur[rule], 2u, rule_scale) &&
            value <= threshold(rules[coord.z].ur[rule], 3u, rule_scale)) {
            next[output_channel] += update_sign(rules[coord.z].us[rule / 16u], (rule * 2u + 1u) & 31u) * step_size * amplitude;
        }
    }

    next -= sigmoid(next, 0.5) * decay_size * 64.0 + decay_size;
    next.a = 1.0;
    imageStore(output_image, coord, clamp(next, 0.0, 1.0));
}
