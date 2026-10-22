#version 460

layout(rgba32f, binding = 0) uniform image2DArray in_tex1;
layout(rgba32f, binding = 1) uniform image2DArray in_tex2;
layout(rgba32f, binding = 2) uniform image2DArray out_tex;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID);
    uint layer = uint(coord.z);
    uint real_layer = layer & ~1u;
    uint imag_layer = real_layer + 1u;
    ivec3 image_size = imageSize(out_tex);

    if (int(imag_layer) >= image_size.z) {
        return;
    }

    ivec3 real_coord = ivec3(coord.xy, int(real_layer));
    ivec3 imag_coord = ivec3(coord.xy, int(imag_layer));

    vec4 a_real = imageLoad(in_tex1, real_coord);
    vec4 a_imag = imageLoad(in_tex1, imag_coord);
    vec4 b_real = imageLoad(in_tex2, real_coord);
    vec4 b_imag = imageLoad(in_tex2, imag_coord);

    vec4 res = (layer == real_layer)
        ? a_real * b_real - a_imag * b_imag
        : a_real * b_imag + a_imag * b_real;

    imageStore(out_tex, coord, res);
}
