#version 460

layout(rgba32f, binding = 0) readonly uniform image2DArray state_image;
layout(rgba32f, binding = 2) writeonly uniform image2DArray output_image;

// Normalized 39-input, 32-Gaussian, 3-output RBF network.
layout(std430, binding = 4) readonly buffer NeuralRule {
    float input_weights[1248];
    float centers[32];
    float widths[32];
    float output_weights[96];
    float output_bias[3];
};

//! option "Time step" exp (0.005, 1.0)
uniform float step_size = 1;
//! option "Diffusion" (0, 1)
uniform float diffusion = 0.0;

//! option "Do modulation"
const bool DO_MODULATION = true;
//! option "Modulation" (0, 1)
uniform float modulation_offset = 0.25;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

const int TILE_SIDE = 40;
const int RADIUS = 12;
shared vec4 tile[1600];

// One quadrant of the integer offsets in r-0.5 <= distance < r+0.5.
// Four rotations cover every ring without sampling the center.
const int RING_START[13] = int[13](0, 2, 5, 9, 17, 24, 34, 44, 56, 73, 87, 105, 122);
const ivec2 RING_OFFSETS[122] = ivec2[122](
    ivec2(1, 0), ivec2(1, 1), ivec2(2, 0), ivec2(2, 1), ivec2(1, 2), ivec2(3, 0), ivec2(3, 1), ivec2(2, 2),
    ivec2(1, 3), ivec2(4, 0), ivec2(4, 1), ivec2(3, 2), ivec2(4, 2), ivec2(2, 3), ivec2(3, 3), ivec2(1, 4),
    ivec2(2, 4), ivec2(5, 0), ivec2(5, 1), ivec2(5, 2), ivec2(4, 3), ivec2(3, 4), ivec2(1, 5), ivec2(2, 5),
    ivec2(6, 0), ivec2(6, 1), ivec2(6, 2), ivec2(5, 3), ivec2(4, 4), ivec2(5, 4), ivec2(3, 5), ivec2(4, 5),
    ivec2(1, 6), ivec2(2, 6), ivec2(7, 0), ivec2(7, 1), ivec2(7, 2), ivec2(6, 3), ivec2(6, 4), ivec2(5, 5),
    ivec2(3, 6), ivec2(4, 6), ivec2(1, 7), ivec2(2, 7), ivec2(8, 0), ivec2(8, 1), ivec2(8, 2), ivec2(7, 3),
    ivec2(7, 4), ivec2(6, 5), ivec2(5, 6), ivec2(6, 6), ivec2(3, 7), ivec2(4, 7), ivec2(1, 8), ivec2(2, 8),
    ivec2(9, 0), ivec2(9, 1), ivec2(9, 2), ivec2(8, 3), ivec2(9, 3), ivec2(8, 4), ivec2(7, 5), ivec2(8, 5),
    ivec2(7, 6), ivec2(5, 7), ivec2(6, 7), ivec2(3, 8), ivec2(4, 8), ivec2(5, 8), ivec2(1, 9), ivec2(2, 9),
    ivec2(3, 9), ivec2(10, 0), ivec2(10, 1), ivec2(10, 2), ivec2(10, 3), ivec2(9, 4), ivec2(9, 5), ivec2(8, 6),
    ivec2(7, 7), ivec2(6, 8), ivec2(4, 9), ivec2(5, 9), ivec2(1, 10), ivec2(2, 10), ivec2(3, 10), ivec2(11, 0),
    ivec2(11, 1), ivec2(11, 2), ivec2(11, 3), ivec2(10, 4), ivec2(10, 5), ivec2(9, 6), ivec2(8, 7), ivec2(9, 7),
    ivec2(7, 8), ivec2(8, 8), ivec2(6, 9), ivec2(7, 9), ivec2(4, 10), ivec2(5, 10), ivec2(1, 11), ivec2(2, 11),
    ivec2(3, 11), ivec2(12, 0), ivec2(12, 1), ivec2(12, 2), ivec2(12, 3), ivec2(11, 4), ivec2(11, 5), ivec2(10, 6),
    ivec2(10, 7), ivec2(9, 8), ivec2(8, 9), ivec2(6, 10), ivec2(7, 10), ivec2(4, 11), ivec2(5, 11), ivec2(1, 12),
    ivec2(2, 12), ivec2(3, 12)
);

vec3 tile_rgb(ivec2 position) {
    return tile[position.y * TILE_SIDE + position.x].rgb;
}

void main() {
    ivec2 size = imageSize(state_image).xy;
    // Keep modulo operands nonnegative, including when a texture is smaller than the halo.
    ivec2 wrap_bias = ((ivec2(RADIUS) + size - 1) / size) * size;
    ivec2 origin = ivec2(gl_WorkGroupID.xy) * 16 - ivec2(RADIUS) + wrap_bias;
    for (uint index = gl_LocalInvocationIndex; index < 1600u; index += 256u) {
        ivec2 position = origin + ivec2(int(index) % TILE_SIDE, int(index) / TILE_SIDE);
        position %= size;
        tile[index] = imageLoad(state_image, ivec3(position, 0));
    }

    // Partial edge workgroups must load their halo and reach this barrier too.
    barrier();
    ivec3 coord = ivec3(gl_GlobalInvocationID);
    if (any(greaterThanEqual(coord, imageSize(output_image))) ||
        any(greaterThanEqual(coord.xy, size)) || coord.z != 0)
        return;

    ivec2 center = ivec2(gl_LocalInvocationID.xy) + ivec2(RADIUS);
    vec3 current = tile_rgb(center);
    float hidden[32];
    for (int neuron = 0; neuron < 32; ++neuron) {
        int base = neuron * 39;
        hidden[neuron] = input_weights[base] * current.r
            + input_weights[base + 1] * current.g
            + input_weights[base + 2] * current.b;
    }

    vec3 near = vec3(0.0);
    for (int ring = 0; ring < 12; ++ring) {
        vec3 sum = vec3(0.0);
        int first = RING_START[ring];
        int end = RING_START[ring + 1];
        for (int sample_index = first; sample_index < end; ++sample_index) {
            ivec2 offset = RING_OFFSETS[sample_index];
            sum += tile_rgb(center + offset);
            sum += tile_rgb(center + ivec2(offset.y, -offset.x));
            sum += tile_rgb(center - offset);
            sum += tile_rgb(center + ivec2(-offset.y, offset.x));
        }
        vec3 average = sum / float(4 * (end - first));
        if (ring == 0)
            near = average;
        // Consume the ring immediately instead of writing a neighborhood texture.
        for (int neuron = 0; neuron < 32; ++neuron) {
            int base = neuron * 39 + 3 + ring * 3;
            hidden[neuron] += input_weights[base] * average.r
                + input_weights[base + 1] * average.g
                + input_weights[base + 2] * average.b;
        }
    }

    for (int neuron = 0; neuron < 32; ++neuron) {
        float distance = (hidden[neuron] - centers[neuron]) / max(widths[neuron], 0.005);
        hidden[neuron] = exp(-distance * distance);
    }

    vec3 rate;
    for (int channel = 0; channel < 3; ++channel) {
        float value = output_bias[channel];
        for (int neuron = 0; neuron < 32; ++neuron)
            value += output_weights[channel * 32 + neuron] * hidden[neuron];
        rate[channel] = value;
    }

    vec3 modulation = current;
    vec3 amplitude = 3.0 * ((modulation - 0.5) * (modulation - 0.5)) + modulation_offset;
    if (DO_MODULATION) {
        rate *= amplitude;
    }

    rate += diffusion * (near - current);
    vec3 next = clamp(current + step_size * rate, 0.0, 1.0);
    imageStore(output_image, coord, vec4(next, 1.0));
}
