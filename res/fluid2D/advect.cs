#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_color(int x, int y) {
    ivec3 image_size = imageSize(inputImage);

    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz) + ivec3(x, y, 0);
    coord = (coord % image_size + image_size) % image_size;
    return (imageLoad(inputImage, coord));
}

void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    vec4 color = get_color(0, 0);

    float dt = 2;
    vec2 pos = -dt * color.xy;
    ivec2 pos0 = ivec2(floor(pos));
    ivec2 pos1 = ivec2(floor(pos)) + 1;
    vec2 scale = pos - pos0;
    float d00 = get_color(pos0.x, pos0.y).z;
    float d01 = get_color(pos0.x, pos1.y).z;
    float d10 = get_color(pos1.x, pos0.y).z;
    float d11 = get_color(pos1.x, pos1.y).z;
    float density = mix(mix(d00, d01, scale.y), mix(d10, d11, scale.y), scale.x);

    color.z = density;
    imageStore(outputImage, coord, color);
}
