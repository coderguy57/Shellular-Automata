#version 460

layout(rgba32f, binding = 0) uniform image2D inputImage;
layout(rgba32f, binding = 1) uniform image2D outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_color(int x, int y, layout (rgba32f) image2D sampler) {
    ivec2 image_size = imageSize(sampler);

    ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
    coord = coord % image_size;
    return (imageLoad(sampler, coord));
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = get_color(0, 0, inputImage);

    float dt = 2;
    vec2 pos = -dt * color.xy;
    ivec2 pos0 = ivec2(floor(pos));
    ivec2 pos1 = ivec2(floor(pos)) + 1;
    vec2 scale = pos - pos0;
    vec2 d00 = get_color(pos0.x, pos0.y, inputImage).zw;
    vec2 d01 = get_color(pos0.x, pos1.y, inputImage).zw;
    vec2 d10 = get_color(pos1.x, pos0.y, inputImage).zw;
    vec2 d11 = get_color(pos1.x, pos1.y, inputImage).zw;
    vec2 density = mix(mix(d00, d01, scale.y), mix(d10, d11, scale.y), scale.x);

    color.zw = density;
    imageStore(outputImage, coord, color);
}