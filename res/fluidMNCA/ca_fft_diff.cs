#version 460

layout(rgba32f, binding = 0) uniform image2DArray in_tex;
layout(rgba32f, binding = 1) uniform image2DArray out_tex;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  vec2 wavenumber = gl_GlobalInvocationID.xy;
  ivec2 image_size = imageSize(in_tex).xy;
  ivec2 middle = image_size / 2;
  wavenumber.x -= wavenumber.x >= middle.x ? image_size.x : 0;
  wavenumber.y -= wavenumber.y >= middle.y ? image_size.y : 0;

  float norm = length(wavenumber);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  vec4 value_real = imageLoad(in_tex, ivec3(coord, 0));
  vec4 value_imag = imageLoad(in_tex, ivec3(coord, 1));
  float factor = 0.00005;
  factor = exp(-factor * norm * norm);
  value_real *= factor;
  value_imag *= factor;

  imageStore(out_tex, ivec3(coord, 0), value_real);
  imageStore(out_tex, ivec3(coord, 1), value_imag);
}
