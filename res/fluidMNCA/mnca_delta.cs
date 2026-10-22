#version 460

layout(rgba32f, binding = 0) uniform image2DArray mncaNextImage;
layout(rgba32f, binding = 1) uniform image2DArray oldImage;
layout(rgba32f, binding = 2) uniform image2DArray outputImage;

//! option "Demand scale" exp (0.1, 100)
uniform float demand_scale = 1.0;

//! option "Use absolute demand"
uniform bool use_absolute_demand = true;

const uint PULL_RADIUS = 4u;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

bool in_pull_neighborhood(ivec2 offset) {
    if (offset == ivec2(0)) {
        return false;
    }

    float radius = float(PULL_RADIUS) + 0.5;
    return length(vec2(offset)) <= radius;
}

float pull_area() {
    float area = 1.0;
    int radius = int(PULL_RADIUS);
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (in_pull_neighborhood(ivec2(x, y))) {
                area += 1.0;
            }
        }
    }
    return area;
}

void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 image_size = imageSize(outputImage);
    if (any(greaterThanEqual(coord, image_size))) {
        return;
    }

    vec4 mnca_next = imageLoad(mncaNextImage, coord);
    vec4 old_data = imageLoad(oldImage, coord);
    vec4 demand = use_absolute_demand ? mnca_next : mnca_next - old_data;
    demand *= demand_scale;
    demand = max(demand, vec4(0.0));
    demand = min(demand, vec4(1.0));
    demand = min(demand, max(vec4(1.0) - old_data, vec4(0.0)));
    demand /= pull_area();
    demand.a = 0.0;

    imageStore(outputImage, coord, demand);
}
