#version 460

layout(rgba32f, binding = 0) uniform image2DArray x_vel_in;
layout(rgba32f, binding = 1) uniform image2DArray y_vel_in;
layout(rgba32f, binding = 2) uniform image2DArray x_vel_out;
layout(rgba32f, binding = 3) uniform image2DArray y_vel_out;

vec4 get_color(int x, int y, layout (rgba32f) image2DArray sampler) {
  ivec2 image_size = imageSize(sampler).xy;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return imageLoad(sampler, ivec3(coord, 0));
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  vec2 wavenumber = gl_GlobalInvocationID.xy;
  ivec2 image_size = imageSize(x_vel_in).xy;
  ivec2 middle = image_size / 2;
  wavenumber.x -= wavenumber.x >= middle.x ? image_size.x : 0;
  wavenumber.y -= wavenumber.y >= middle.y ? image_size.y : 0;

  float norm = length(wavenumber);
  wavenumber /= (norm > 0) ? norm : 1;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  vec4 vel_x_real = imageLoad(x_vel_in, ivec3(coord, 0));
  vec4 vel_x_imag = imageLoad(x_vel_in, ivec3(coord, 1));
  vec4 vel_y_real = imageLoad(y_vel_in, ivec3(coord, 0));
  vec4 vel_y_imag = imageLoad(y_vel_in, ivec3(coord, 1));
  vec4 pressure_real = vel_x_real * wavenumber.x + vel_y_real * wavenumber.y;
  vec4 pressure_imag = vel_x_imag * wavenumber.x + vel_y_imag * wavenumber.y;
  vel_x_real -= pressure_real * wavenumber.x;
  vel_x_imag -= pressure_imag * wavenumber.x;
  vel_y_real -= pressure_real * wavenumber.y;
  vel_y_imag -= pressure_imag * wavenumber.y;

  imageStore(x_vel_out, ivec3(coord, 0), vel_x_real);
  imageStore(x_vel_out, ivec3(coord, 1), vel_x_imag);
  imageStore(y_vel_out, ivec3(coord, 0), vel_y_real);
  imageStore(y_vel_out, ivec3(coord, 1), vel_y_imag);
}