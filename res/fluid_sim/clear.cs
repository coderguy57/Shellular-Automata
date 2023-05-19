#version 460

layout(rgba32f, binding = 0) uniform image2D outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = vec4(0);
  imageStore(outputImage, coord, color);
}