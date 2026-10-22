#version 460

layout(rgba32f, binding = 0) uniform image2D inputImage;
layout(rgba32f, binding = 1) uniform image2D defaultImage;
layout(rgba32f, binding = 2) uniform image2D outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_input_color(int x, int y) {
  ivec2 image_size = imageSize(inputImage);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = (coord % image_size + image_size) % image_size;
  return imageLoad(inputImage, coord);
}

vec4 get_default_color(int x, int y) {
  ivec2 image_size = imageSize(defaultImage);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = (coord % image_size + image_size) % image_size;
  return imageLoad(defaultImage, coord);
}

void main() {
  vec4 density = get_default_color(0, 0);
  vec4 left_density = get_input_color(-1, 0);
  vec4 right_density = get_input_color(1, 0);
  vec4 bottom_density = get_input_color(0, -1);
  vec4 top_density = get_input_color(0, 1);

  vec4 sum = left_density + right_density + bottom_density + top_density;
  density = (density + sum) / 4;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  imageStore(outputImage, coord, density);
}
