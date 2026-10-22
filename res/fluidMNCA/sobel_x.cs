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

vec4 doCalc() {
    vec4 colul = -1*get_color(-1, -1, 0);
    vec4 colml = -2*get_color(-1, 0, 0);
    vec4 coldl = -1*get_color(-1, 1, 0);

    vec4 colur = +1*get_color(1, -1, 0);
    vec4 colmr = +2*get_color(1, 0, 0);
    vec4 coldr = +1*get_color(1, 1, 0);

    vec4 res = colul + colml + coldl + colur + colmr + coldr;
    return res / 8;
}

void main() {
    vec4 res = doCalc();
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

    vec4 vel = imageLoad(vel_in, ivec3(coord, 0));
    imageStore(outputImage, ivec3(coord, 0), vel + res * force_scale);
}
