#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputOutputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
  ivec3 coord = ivec3(gl_GlobalInvocationID);
  ivec2 image_size = imageSize(inputOutputImage).xy;

  vec4 color;
  // float value = length(vec2(coord - image_size / 2)) / 128;
  // value = max(step(0, 1 - value), 0);

  int numSquares = 8;
  ivec2 squareSize = image_size / numSquares;

  // Calculate the checkerboard indices for each square
  ivec2 squareIndex = coord.xy / squareSize;

  // Calculate the temperature based on the checkerboard indices
  float temperature = mod(float(squareIndex.x + squareIndex.y), 2.0);

  vec2 middle = vec2(coord.xy) - vec2(image_size.x / 2, image_size.y / 5 * 4);
  // middle = normalize(middle);
  float value = length(middle);
  float test = step(abs(value - 32), 5);
  float test2 = step(value, 32);
  middle *= test;

  // color = vec4(vec3(0, 0, temperature), 1);
  color = vec4(vec3(0, 0, 0), 0);
  imageStore(inputOutputImage, coord, color);
}
