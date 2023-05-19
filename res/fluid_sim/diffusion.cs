#version 460

layout(rgba32f, binding = 0) uniform image2D inputImage;
layout(rgba32f, binding = 1) uniform image2D defaultImage;
layout(rgba32f, binding = 2) uniform image2D outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_color(int x, int y, layout (rgba32f) image2D sampler) {
  ivec2 image_size = imageSize(sampler);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return (imageLoad(sampler, coord));
}

const vec4 diffusion_coefficient = vec4(0, 0, 0.1, 0);
void main() {
  vec4 density = get_color(0, 0, defaultImage);
  vec4 left_density = get_color(-1, 0, inputImage);
  vec4 right_density = get_color(1, 0, inputImage);
  vec4 bottom_density = get_color(0, -1, inputImage);
  vec4 top_density = get_color(0, 1, inputImage);

  vec4 sum = left_density + right_density + bottom_density + top_density;
  density = (density + diffusion_coefficient * sum) / (1+4*diffusion_coefficient);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = imageLoad(defaultImage, coord);
  color = density;
  imageStore(outputImage, coord, color);
}
