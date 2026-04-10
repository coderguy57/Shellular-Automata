#version 460

layout(rgba32f, binding = 0) uniform image2DArray out_tex;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  vec2 wavenumber = gl_GlobalInvocationID.xy;
  ivec2 image_size = imageSize(out_tex).xy;
  ivec2 middle = image_size / 2;
  wavenumber.x -= wavenumber.x >= middle.x ? image_size.x : 0;
  wavenumber.y -= wavenumber.y >= middle.y ? image_size.y : 0;
  // wavenumber.x -= middle.x / 2;
  // wavenumber.y -= middle.y / 2;

  float norm = length(wavenumber);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  float factor = 1.0/128.0;
  factor = exp(-factor * norm * norm);
  vec4 value = vec4(1.0, 1.0, 1.0, 1.0);
  // factor = sin(2 * 3.14159265359 * (wavenumber.y+wavenumber.x) / 32.0);
  factor = max(abs(wavenumber.x), abs(wavenumber.y)) < 16 ? 1.0 : 0.0;
  value.rgb *= factor / 1000;

  imageStore(out_tex, ivec3(coord, 0), value);
}