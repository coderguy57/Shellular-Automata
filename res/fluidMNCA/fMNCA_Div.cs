#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_color(int x, int y, layout (rgba32f) image2D sampler, int layer) {
  ivec2 image_size = imageSize(sampler);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return (imageLoad(sampler, ivec3(coord, layer).xy);
}

const float diffusion_coefficient = 0.1;
void main() {
  vec4 left_density = get_color(-1, 0, inputImage, 1);
  vec4 right_density = get_color(1, 0, inputImage, 1);
  vec4 bottom_density = get_color(0, -1, inputImage, 2);
  vec4 top_density = get_color(0, 1, inputImage, 2);

  ivec2 image_size = imageSize(inputImage);
  vec4 diff = -0.5*((right_density - left_density)/image_size + (top_density - bottom_density)/image_size);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  imageStore(outputImage, coord, diff);
}
