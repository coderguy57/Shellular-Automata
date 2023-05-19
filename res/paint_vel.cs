#version 460

layout(rgba32f, binding = 0) uniform image2D inputImage;
layout(rgba32f, binding = 1) uniform image2D outputImage;

uniform vec2 cursor_pos;
uniform vec2 last_cursor_pos;
uniform ivec2 mlr;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  ivec2 image_size = imageSize(inputImage);

  vec4 color = imageLoad(inputImage, coord);

  float dist = length((cursor_pos * image_size) - coord);
  float paint = max(0, 10 - dist);
  float before = max(1, color.z);
  color.z += mlr.x * paint * (coord.x > (image_size.x/2)? 1 : 0);
  color.z = min(before, color.z);
  color.w += mlr.x * paint * (coord.x < (image_size.x/2)? 1 : 0);
  color.w = min(before, color.w);
  
  paint = step(0.0001, max(0, 10 - dist));
  vec2 dir = cursor_pos - last_cursor_pos;
  color.xy += 100 * mlr.y * paint * dir;
  color.xy = max(vec2(-0.5), min(vec2(0.5), color.xy));
  imageStore(outputImage, coord, color);
}