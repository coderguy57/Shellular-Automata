#version 460

layout(rgba32f, binding = 0) uniform image2DArray dataImage;
layout(rgba32f, binding = 1) uniform image2DArray demandImage;
layout(rgba32f, binding = 2) uniform image2DArray totalOutImage;
layout(rgba32f, binding = 3) uniform image2DArray outputImage;

const uint FLUX_RADIUS = 4u;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

ivec2 wrap_coord(ivec2 coord) {
    ivec2 image_size = imageSize(outputImage).xy;
    return (coord % image_size + image_size) % image_size;
}

vec4 load_data(ivec2 coord) {
    return imageLoad(dataImage, ivec3(wrap_coord(coord), 0));
}

vec4 load_demand(ivec2 coord) {
    vec4 demand = max(imageLoad(demandImage, ivec3(wrap_coord(coord), 0)), vec4(0.0));
    demand.a = 0.0;
    return demand;
}

vec4 safe_div(vec4 numerator, vec4 denominator) {
    vec4 result = vec4(0.0);
    for (uint i = 0u; i < 4u; ++i) {
        result[i] = denominator[i] > 0.0 ? numerator[i] / denominator[i] : 0.0;
    }
    return result;
}

bool in_flux_neighborhood(ivec2 offset) {
    if (offset == ivec2(0)) {
        return false;
    }

    float radius = float(FLUX_RADIUS) + 0.5;
    return length(vec2(offset)) <= radius;
}

vec4 load_total_out(ivec2 coord) {
    vec4 total = imageLoad(totalOutImage, ivec3(wrap_coord(coord), 0));
    total.a = 0.0;
    return total;
}

vec4 flux_between(ivec2 source_coord, ivec2 target_coord) {
    vec4 target_demand = load_demand(target_coord);
    vec4 total = load_total_out(source_coord);
    vec4 source = max(load_data(source_coord), vec4(0.0));
    source.a = 0.0;

    vec4 used_supply = min(source, total);

    return target_demand * safe_div(used_supply, total);
}

void main() {
    ivec3 coord3 = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 image_size = imageSize(outputImage);
    if (any(greaterThanEqual(coord3, image_size))) {
        return;
    }

    ivec2 coord = coord3.xy;
    vec4 result = load_data(coord);

    int radius = int(FLUX_RADIUS);
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            ivec2 offset = ivec2(x, y);
            if (in_flux_neighborhood(offset)) {
                ivec2 neighbor = coord + offset;
                result += flux_between(neighbor, coord);
                result -= flux_between(coord, neighbor);
            }
        }
    }

    result.a = 1.0;
    imageStore(outputImage, coord3, result);
}
