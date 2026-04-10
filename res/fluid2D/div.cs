#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec2 get_color(int x, int y, layout (rgba32f) image2DArray sampler) {
  ivec3 image_size = imageSize(sampler);

  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz) + ivec3(x, y, 0);
  coord = coord % image_size;
  return (imageLoad(sampler, coord).xy);
}

const float diffusion_coefficient = 0.1;
void main() {
  vec2 left_density = get_color(-1, 0, inputImage);
  vec2 right_density = get_color(1, 0, inputImage);
  vec2 bottom_density = get_color(0, -1, inputImage);
  vec2 top_density = get_color(0, 1, inputImage);

  ivec3 image_size = imageSize(inputImage);
  float diff = -0.5*((right_density.x - left_density.x)/image_size.x + (top_density.y - bottom_density.y)/image_size.y);

  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
  vec4 color = imageLoad(inputImage, coord);
  color = vec4(diff, 0, 0, 0);
  imageStore(outputImage, coord, color);
}
