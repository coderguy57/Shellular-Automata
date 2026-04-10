#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray changeImage;
layout(rgba32f, binding = 2) uniform image2DArray outputImage;

float get_color(int x, int y, layout (rgba32f) image2DArray sampler) {
  ivec3 image_size = imageSize(sampler);

  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz) + ivec3(x, y, 0);
  coord = coord % image_size;
  return (imageLoad(sampler, coord).x);
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
  ivec3 image_size = imageSize(changeImage);
  vec4 color = imageLoad(inputImage, coord);

  color.x -= 0.5*image_size.x*(get_color(1,0, changeImage) - get_color(-1, 0, changeImage));
  color.y -= 0.5*image_size.y*(get_color(0,1, changeImage) - get_color(0, -1, changeImage));
  imageStore(outputImage, coord, color);
}