#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray defaultImage;
layout(rgba32f, binding = 2) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

float get_input_color(int x, int y) {
  ivec3 image_size = imageSize(inputImage);

  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz) + ivec3(x, y, 0);
  coord = (coord % image_size + image_size) % image_size;
  return (imageLoad(inputImage, coord).x);
}

float get_default_color(int x, int y) {
  ivec3 image_size = imageSize(defaultImage);

  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz) + ivec3(x, y, 0);
  coord = (coord % image_size + image_size) % image_size;
  return (imageLoad(defaultImage, coord).x);
}

void main() {
  float density = get_default_color(0, 0);
  float left_density = get_input_color(-1, 0);
  float right_density = get_input_color(1, 0);
  float bottom_density = get_input_color(0, -1);
  float top_density = get_input_color(0, 1);

  float sum = left_density + right_density + bottom_density + top_density;
  density = (density + sum) / 4;

  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
  vec4 color = imageLoad(defaultImage, coord);
  color.x = density;
  imageStore(outputImage, coord, color);
}
