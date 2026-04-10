#version 460

layout(rgba32f, binding = 0) uniform image2DArray x_vel_in;
layout(rgba32f, binding = 1) uniform image2DArray y_vel_in;
layout(rgba32f, binding = 2) uniform image2DArray inputImage;
layout(rgba32f, binding = 3) uniform image2DArray outputImage;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 get_color(int x, int y, layout (rgba32f) image2DArray sampler, int layer) {
    ivec2 image_size = imageSize(sampler).xy;

    ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
    coord = coord % image_size;
    return imageLoad(sampler, ivec3(coord, layer));
}

vec4 get_color(ivec4 x, ivec4 y, layout (rgba32f) image2DArray sampler, int layer) {
    vec4 color;
    for (int i = 0; i < 4; i++) {
        color[i] = get_color(x[i], y[i], sampler, layer)[i];
    }
    return color;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 colorX = get_color(0, 0, x_vel_in, 0);
    vec4 colorY = get_color(0, 0, y_vel_in, 0);

    float dt = 2000;
    vec4 posX = -dt * colorX;
    vec4 posY = -dt * colorY;
    ivec4 pos0X = ivec4(floor(posX));
    ivec4 pos1X = ivec4(floor(posX)) + 1;
    ivec4 pos0Y = ivec4(floor(posY));
    ivec4 pos1Y = ivec4(floor(posY)) + 1;
    vec4 scaleX = posX - pos0X;
    vec4 scaleY = posY - pos0Y;

    vec4 d00 = get_color(pos0X, pos0Y, inputImage, 0);
    vec4 d01 = get_color(pos0X, pos1Y, inputImage, 0);
    vec4 d10 = get_color(pos1X, pos0Y, inputImage, 0);
    vec4 d11 = get_color(pos1X, pos1Y, inputImage, 0);

    vec4 color = mix(mix(d00, d01, scaleY), mix(d10, d11, scaleY), scaleX);

    imageStore(outputImage, ivec3(coord, 0), color);
}