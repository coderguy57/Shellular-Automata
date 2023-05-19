#version 460

layout(rgba32f, binding = 0) uniform image2D inputImage;
layout(rgba32f, binding = 1) uniform image2D changeImage;
layout(rgba32f, binding = 2) uniform image2D outputImage;

float get_color(int x, int y, layout (rgba32f) image2D sampler) {
  ivec2 image_size = imageSize(sampler);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return (imageLoad(sampler, coord).x);
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  ivec2 image_size = imageSize(changeImage);
  vec4 color = imageLoad(inputImage, coord);

  float value = length(vec2(coord - image_size / 2));
  float test = step(128, value);
  color.x -= 0.5*image_size.x*(get_color(1,0, changeImage) - get_color(-1, 0, changeImage));
  color.y -= 0.5*image_size.y*(get_color(0,1, changeImage) - get_color(0, -1, changeImage));
  imageStore(outputImage, coord, color);
}