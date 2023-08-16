#version 460

layout(rgba32f, binding = 0) uniform image2D inputImage;
layout(rgba32f, binding = 1) uniform image2D defaultImage;
layout(rgba32f, binding = 2) uniform image2D outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

float get_color(int x, int y, layout (rgba32f) image2D sampler) {
  ivec2 image_size = imageSize(sampler);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  float value = length(vec2(coord - image_size / 2));
  float test = step(128, value);
  coord = coord % image_size;
  return (imageLoad(sampler, coord).x);
}

void main() {
  float density = get_color(0, 0, defaultImage);
  float left_density = get_color(-1, 0, inputImage);
  float right_density = get_color(1, 0, inputImage);
  float bottom_density = get_color(0, -1, inputImage);
  float top_density = get_color(0, 1, inputImage);

  float sum = left_density + right_density + bottom_density + top_density;
  density = (density + sum) / 4;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = imageLoad(defaultImage, coord);
  color.x = density;
  imageStore(outputImage, coord, color);
}
