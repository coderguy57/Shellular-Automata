#version 460

layout(rgba32f, binding = 0) uniform image2DArray in_tex1;
layout(rgba32f, binding = 1) uniform image2DArray in_tex2;
layout(rgba32f, binding = 2) uniform image2DArray out_tex;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID);

    vec4 value1 = imageLoad(in_tex1, coord);
    vec4 value2 = imageLoad(in_tex2, coord);

    vec4 res = value1 * value2;

    ivec2 imageSize = ivec2(imageSize(out_tex).xy);


    imageStore(out_tex, coord, res);
}