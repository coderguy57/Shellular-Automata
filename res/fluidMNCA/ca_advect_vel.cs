#version 460

layout(rgba32f, binding = 0) uniform image2DArray x_vel_in;
layout(rgba32f, binding = 1) uniform image2DArray y_vel_in;
layout(rgba32f, binding = 2) uniform image2DArray x_vel_out;
layout(rgba32f, binding = 3) uniform image2DArray y_vel_out;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

ivec2 wrap_coord(ivec2 coord, ivec2 image_size) {
    return (coord % image_size + image_size) % image_size;
}

vec4 get_x_vel(int x, int y) {
    ivec2 image_size = imageSize(x_vel_in).xy;

    ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
    coord = wrap_coord(coord, image_size);
    return imageLoad(x_vel_in, ivec3(coord, 0));
}

vec4 get_y_vel(int x, int y) {
    ivec2 image_size = imageSize(y_vel_in).xy;

    ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(x, y);
    coord = wrap_coord(coord, image_size);
    return imageLoad(y_vel_in, ivec3(coord, 0));
}

vec4 get_x_vel(ivec4 x, ivec4 y) {
    vec4 color;
    for (int i = 0; i < 4; i++) {
        color[i] = get_x_vel(x[i], y[i])[i];
    }
    return color;
}

vec4 get_y_vel(ivec4 x, ivec4 y) {
    vec4 color;
    for (int i = 0; i < 4; i++) {
        color[i] = get_y_vel(x[i], y[i])[i];
    }
    return color;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 x_vel = get_x_vel(0, 0);
    vec4 y_vel = get_y_vel(0, 0);

    float dt = 2000;
    vec4 posX = -dt * x_vel;
    vec4 posY = -dt * y_vel;
    ivec4 pos0X = ivec4(floor(posX));
    ivec4 pos1X = ivec4(floor(posX)) + 1;
    ivec4 pos0Y = ivec4(floor(posY));
    ivec4 pos1Y = ivec4(floor(posY)) + 1;
    vec4 scaleX = posX - pos0X;
    vec4 scaleY = posY - pos0Y;

    vec4 d00X = get_x_vel(pos0X, pos0Y);
    vec4 d01X = get_x_vel(pos0X, pos1Y);
    vec4 d10X = get_x_vel(pos1X, pos0Y);
    vec4 d11X = get_x_vel(pos1X, pos1Y);

    vec4 d00Y = get_y_vel(pos0X, pos0Y);
    vec4 d01Y = get_y_vel(pos0X, pos1Y);
    vec4 d10Y = get_y_vel(pos1X, pos0Y);
    vec4 d11Y = get_y_vel(pos1X, pos1Y);

    vec4 densityX = mix(mix(d00X, d01X, scaleY), mix(d10X, d11X, scaleY), scaleX);
    vec4 densityY = mix(mix(d00Y, d01Y, scaleY), mix(d10Y, d11Y, scaleY), scaleX);

    imageStore(x_vel_out, ivec3(coord, 0), densityX);
    imageStore(y_vel_out, ivec3(coord, 0), densityY);
}
