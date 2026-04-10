#version 460

layout(rgba32f, binding = 0) uniform image2DArray x_vel_in;
layout(rgba32f, binding = 1) uniform image2DArray y_vel_in;
layout(rgba32f, binding = 2) uniform image2DArray changeImage;
layout(rgba32f, binding = 3) uniform image2DArray x_vel_out;
layout(rgba32f, binding = 4) uniform image2DArray y_vel_out;

vec4 get_color(int x, int y, layout (rgba32f) image2DArray sampler) {
  ivec2 image_size = imageSize(sampler).xy;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return imageLoad(sampler, ivec3(coord, 0));
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  ivec2 image_size = imageSize(changeImage).xy;
  vec4 vel_x = imageLoad(x_vel_in, ivec3(coord, 0));
  vec4 vel_y = imageLoad(y_vel_in, ivec3(coord, 0));

  vel_x -= 0.5*image_size.x*(get_color(1,0, changeImage) - get_color(-1, 0, changeImage));
  vel_y -= 0.5*image_size.y*(get_color(0,1, changeImage) - get_color(0, -1, changeImage));

  imageStore(x_vel_out, ivec3(coord, 0), vel_x);
  imageStore(y_vel_out, ivec3(coord, 0), vel_y);
}