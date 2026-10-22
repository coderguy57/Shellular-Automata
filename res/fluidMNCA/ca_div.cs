#version 460

layout(rgba32f, binding = 0) uniform image2DArray x_vel;
layout(rgba32f, binding = 1) uniform image2DArray y_vel;
layout(rgba32f, binding = 2) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_x_vel(int x, int y) {
  ivec2 image_size = imageSize(x_vel).xy;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = (coord % image_size + image_size) % image_size;
  return imageLoad(x_vel, ivec3(coord, 0));
}

vec4 get_y_vel(int x, int y) {
  ivec2 image_size = imageSize(y_vel).xy;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = (coord % image_size + image_size) % image_size;
  return imageLoad(y_vel, ivec3(coord, 0));
}

void main() {
  vec4 left_density = get_x_vel(-1, 0);
  vec4 right_density = get_x_vel(1, 0);
  vec4 bottom_density = get_y_vel(0, -1);
  vec4 top_density = get_y_vel(0, 1);

  ivec2 image_size = imageSize(outputImage).xy;
  vec4 diff = -0.5*((right_density - left_density)/image_size.x + (top_density - bottom_density)/image_size.y);

  ivec3 coord = ivec3(gl_GlobalInvocationID.xy, 0);
  imageStore(outputImage, coord, diff);
}
