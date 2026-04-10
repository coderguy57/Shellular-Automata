#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

//! option "layer_in"
const uint LAYER_IN = 0u;

//! option "layer_out"
const uint LAYER_OUT = 0u;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = imageLoad(inputImage, ivec3(coord, LAYER_IN));
  imageStore(outputImage, ivec3(coord, LAYER_OUT), color);
}