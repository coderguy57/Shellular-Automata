#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray defaultImage;
layout(rgba32f, binding = 2) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_color1(int x, int y, int layer) {
  ivec2 image_size = imageSize(defaultImage).xy;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return imageLoad(defaultImage, ivec3(coord, layer));
}

vec4 get_color2(int x, int y, int layer) {
  ivec2 image_size = imageSize(inputImage).xy;

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
  coord = coord % image_size;
  return imageLoad(inputImage, ivec3(coord, layer));
}

const vec4 diffusion_coefficient = vec4(5);
vec4 doCalc(int layer) {
  vec4 density = get_color1(0, 0, layer);
  vec4 left_density = get_color2(-1, 0, layer);
  vec4 right_density = get_color2(1, 0, layer);
  vec4 bottom_density = get_color2(0, -1, layer);
  vec4 top_density = get_color2(0, 1, layer);

  vec4 sum = left_density + right_density + bottom_density + top_density;
  density = (density + diffusion_coefficient * sum) / (1+4*diffusion_coefficient);

  return density;
}

void main() {
  vec4 color0 = get_color2(0, 0, 0);
  vec4 colorX = doCalc(1);
  vec4 colorY = doCalc(2);

  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

  imageStore(outputImage, ivec3(coord, 0), color0);
  imageStore(outputImage, ivec3(coord, 1), colorX);
  imageStore(outputImage, ivec3(coord, 2), colorY);
}
