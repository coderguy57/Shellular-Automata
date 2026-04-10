#version 460

layout(rgba32f, binding = 0) uniform image2DArray x_vel_in;
layout(rgba32f, binding = 1) uniform image2DArray y_vel_in;
layout(rgba32f, binding = 2) uniform image2DArray x_vel_out;
layout(rgba32f, binding = 3) uniform image2DArray y_vel_out;

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
    vec4 x_vel = get_color(0, 0, x_vel_in, 0);
    vec4 y_vel = get_color(0, 0, y_vel_in, 0);

    float dt = 2000;
    vec4 posX = -dt * x_vel;
    vec4 posY = -dt * y_vel;
    ivec4 pos0X = ivec4(floor(posX));
    ivec4 pos1X = ivec4(floor(posX)) + 1;
    ivec4 pos0Y = ivec4(floor(posY));
    ivec4 pos1Y = ivec4(floor(posY)) + 1;
    vec4 scaleX = posX - pos0X;
    vec4 scaleY = posY - pos0Y;

    vec4 d00X = get_color(pos0X, pos0Y, x_vel_in, 0);
    vec4 d01X = get_color(pos0X, pos1Y, x_vel_in, 0);
    vec4 d10X = get_color(pos1X, pos0Y, x_vel_in, 0);
    vec4 d11X = get_color(pos1X, pos1Y, x_vel_in, 0);

    vec4 d00Y = get_color(pos0X, pos0Y, y_vel_in, 0);
    vec4 d01Y = get_color(pos0X, pos1Y, y_vel_in, 0);
    vec4 d10Y = get_color(pos1X, pos0Y, y_vel_in, 0);
    vec4 d11Y = get_color(pos1X, pos1Y, y_vel_in, 0);

    vec4 densityX = mix(mix(d00X, d01X, scaleY), mix(d10X, d11X, scaleY), scaleX);
    vec4 densityY = mix(mix(d00Y, d01Y, scaleY), mix(d10Y, d11Y, scaleY), scaleX);

    imageStore(x_vel_out, ivec3(coord, 0), densityX);
    imageStore(y_vel_out, ivec3(coord, 0), densityY);
}