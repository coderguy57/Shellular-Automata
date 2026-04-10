#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 3) in;
void main() {
  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
  vec4 color = imageLoad(inputImage, coord);
  imageStore(outputImage, coord, color);
}