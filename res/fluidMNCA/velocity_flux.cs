#version 460

layout(rgba32f, binding = 0) uniform image2DArray xVelocityImage;
layout(rgba32f, binding = 1) uniform image2DArray yVelocityImage;
layout(rgba32f, binding = 2) uniform image2DArray dataImage;
layout(rgba32f, binding = 3) uniform image2DArray outputImage;

//! option "Velocity scale" exp (1, 5000)
uniform float velocity_scale = 2000.0;

//! option "Velocity flux fraction" (0, 1)
uniform float velocity_flux_fraction = 0.95;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

ivec2 wrap_coord(ivec2 coord) {
    ivec2 image_size = imageSize(outputImage).xy;
    return (coord % image_size + image_size) % image_size;
}

vec4 load_data(ivec2 coord) {
    return imageLoad(dataImage, ivec3(wrap_coord(coord), 0));
}

vec4 load_x_velocity(ivec2 coord) {
    return imageLoad(xVelocityImage, ivec3(wrap_coord(coord), 0));
}

vec4 load_y_velocity(ivec2 coord) {
    return imageLoad(yVelocityImage, ivec3(wrap_coord(coord), 0));
}

vec4 safe_div(vec4 numerator, vec4 denominator) {
    vec4 result = vec4(0.0);
    for (uint i = 0u; i < 4u; ++i) {
        result[i] = denominator[i] > 0.0 ? numerator[i] / denominator[i] : 0.0;
    }
    return result;
}

vec4 raw_velocity_out(ivec2 source_coord, ivec2 offset) {
    if (offset == ivec2(1, 0)) {
        vec4 u = 0.5 * (load_x_velocity(source_coord) + load_x_velocity(source_coord + offset));
        return max(u * velocity_scale, vec4(0.0));
    }
    if (offset == ivec2(-1, 0)) {
        vec4 u = 0.5 * (load_x_velocity(source_coord) + load_x_velocity(source_coord + offset));
        return max(-u * velocity_scale, vec4(0.0));
    }
    if (offset == ivec2(0, 1)) {
        vec4 v = 0.5 * (load_y_velocity(source_coord) + load_y_velocity(source_coord + offset));
        return max(v * velocity_scale, vec4(0.0));
    }
    if (offset == ivec2(0, -1)) {
        vec4 v = 0.5 * (load_y_velocity(source_coord) + load_y_velocity(source_coord + offset));
        return max(-v * velocity_scale, vec4(0.0));
    }
    return vec4(0.0);
}

vec4 total_raw_out(ivec2 source_coord) {
    vec4 total = vec4(0.0);
    total += raw_velocity_out(source_coord, ivec2(1, 0));
    total += raw_velocity_out(source_coord, ivec2(-1, 0));
    total += raw_velocity_out(source_coord, ivec2(0, 1));
    total += raw_velocity_out(source_coord, ivec2(0, -1));
    total.a = 0.0;
    return total;
}

vec4 flux_between(ivec2 source_coord, ivec2 target_coord) {
    ivec2 offset = target_coord - source_coord;
    vec4 raw = raw_velocity_out(source_coord, offset);
    raw.a = 0.0;

    vec4 total = total_raw_out(source_coord);
    vec4 source = max(load_data(source_coord), vec4(0.0));
    source.a = 0.0;

    vec4 out_fraction = min(total, vec4(clamp(velocity_flux_fraction, 0.0, 1.0)));
    vec4 actual_out = source * out_fraction;

    return raw * safe_div(actual_out, total);
}

void main() {
    ivec3 coord3 = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 image_size = imageSize(outputImage);
    if (any(greaterThanEqual(coord3, image_size))) {
        return;
    }

    ivec2 coord = coord3.xy;
    vec4 result = load_data(coord);

    result += flux_between(coord + ivec2(1, 0), coord);
    result += flux_between(coord + ivec2(-1, 0), coord);
    result += flux_between(coord + ivec2(0, 1), coord);
    result += flux_between(coord + ivec2(0, -1), coord);

    result -= flux_between(coord, coord + ivec2(1, 0));
    result -= flux_between(coord, coord + ivec2(-1, 0));
    result -= flux_between(coord, coord + ivec2(0, 1));
    result -= flux_between(coord, coord + ivec2(0, -1));

    result = max(result, vec4(0.0));
    result.a = 1.0;
    imageStore(outputImage, coord3, result);
}
