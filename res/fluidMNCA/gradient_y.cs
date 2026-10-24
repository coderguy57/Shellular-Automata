#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray vel_in;
layout(rgba32f, binding = 2) uniform image2DArray outputImage;

//! option "Force scale" exp (0.01, 100)
uniform float force_scale = 1.1;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_color(int x, int y, int layer) {
    ivec2 image_size = imageSize(inputImage).xy;

    ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
    coord = (coord % image_size + image_size) % image_size;
    return imageLoad(inputImage, ivec3(coord, layer));
}

vec4 gradient() {
    vec4 colul = -3*get_color(-1, -1, 0);
    vec4 colum = -10*get_color(0, -1, 0);
    vec4 colur = -3*get_color(1, -1, 0);

    vec4 coldl = +3*get_color(-1, 1, 0);
    vec4 coldm = +10*get_color(0, 1, 0);
    vec4 coldr = +3*get_color(1, 1, 0);

    vec4 res = colul + colum + colur + coldl + coldm + coldr;
    // Normalized Scharr derivative: a unit ramp produces a unit gradient.
    return res / 32;
}

void main() {
    if (any(greaterThanEqual(ivec3(gl_GlobalInvocationID), imageSize(outputImage)))) {
        return;
    }
    vec4 res = gradient();
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

    vec4 vel = imageLoad(vel_in, ivec3(coord, 0));
    imageStore(outputImage, ivec3(coord, 0), vel + res * force_scale);
}
