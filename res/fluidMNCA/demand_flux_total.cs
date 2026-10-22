#version 460

layout(rgba32f, binding = 0) uniform image2DArray demandImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

const uint FLUX_RADIUS = 4u;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

ivec2 wrap_coord(ivec2 coord) {
    ivec2 image_size = imageSize(outputImage).xy;
    return (coord % image_size + image_size) % image_size;
}

vec4 load_demand(ivec2 coord) {
    vec4 demand = max(imageLoad(demandImage, ivec3(wrap_coord(coord), 0)), vec4(0.0));
    demand.a = 0.0;
    return demand;
}

bool in_flux_neighborhood(ivec2 offset) {
    if (offset == ivec2(0)) {
        return false;
    }

    float radius = float(FLUX_RADIUS) + 0.5;
    return length(vec2(offset)) <= radius;
}

void main() {
    ivec3 coord3 = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 image_size = imageSize(outputImage);
    if (any(greaterThanEqual(coord3, image_size))) {
        return;
    }

    ivec2 coord = coord3.xy;
    vec4 total = load_demand(coord);

    int radius = int(FLUX_RADIUS);
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            ivec2 offset = ivec2(x, y);
            if (in_flux_neighborhood(offset)) {
                total += load_demand(coord + offset);
            }
        }
    }

    total.a = 0.0;
    imageStore(outputImage, coord3, total);
}
