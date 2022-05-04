#version 460

layout(binding = 5) buffer out_buffer_r {int red[];};
layout(binding = 6) buffer out_buffer_b {int blue[];};
layout(binding = 7) buffer out_buffer_g {int green[];};

uniform sampler2DArray tex;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    uvec2 coord = gl_GlobalInvocationID.xy;
    vec3 color = texelFetch(tex, ivec3(coord, 0), 0).xyz;
    ivec3 index = ivec3(color * 255);

    atomicAdd(red[index.r], 1);
    atomicAdd(blue[index.b], 1);
    atomicAdd(green[index.g], 1);

    // Second layer
    // color = texelFetch(tex, ivec3(coord, 0), 1).xyz;
    // index = ivec3(color * 255);

    // atomicAdd(red[index.r], 1);
    // atomicAdd(blue[index.b], 1);
    // atomicAdd(green[index.g], 1);
}