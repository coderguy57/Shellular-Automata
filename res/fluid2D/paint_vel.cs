#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

uniform vec3 paint_mask;
uniform vec2 cursor_pos;
uniform vec2 last_cursor_pos;
uniform ivec2 mlr;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
  ivec3 image_size = imageSize(inputImage);

  vec4 color = imageLoad(inputImage, coord);

  float mask = paint_mask[coord.z];

  float dist = length((cursor_pos * image_size.xy) - coord.xy);
  float paint = max(0, 10 - dist);
  float before = max(1, color.z);
  color.z += mlr.x * paint * mask;
  color.z = min(before, color.z);
  
  paint = step(0.0001, max(0, 10 - dist));
  vec2 dir = cursor_pos - last_cursor_pos;
  color.xy += 100 * mlr.y * paint * dir * mask;
  color.xy = max(vec2(-0.5), min(vec2(0.5), color.xy));
  imageStore(outputImage, coord, color);
}