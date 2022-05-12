#version 460

uniform sampler2DArray in_tex;
writeonly uniform image2D out_tex;

#define SIZE 1024

#define PI 3.14159265358979323844

shared vec2 values[SIZE][2][3];

void synchronize()
{
    memoryBarrierShared();
    barrier();
}

void
fft_pass(int ns, int source)
{
    uint i = gl_LocalInvocationID.x;

    uint base = (i/ns)*(ns/2);
    uint offs = i%(ns/2);

    uint i0 = base + offs;
    uint i1 = i0 + SIZE/2;
    
    vec2[3] v0 = values[i0][source];
    vec2[3] v1 = values[i1][source];
    
    float a = -2.*PI*float(i)/ns;

    float t_re = cos(a);
    float t_im = sin(a);

    for (uint channel = 0; channel < 3; channel++) {
        values[i][source ^ 1][channel] = v0[channel] 
            + vec2(dot(vec2(t_re, -t_im), v1[channel]), dot(vec2(t_im, t_re), v1[channel]));
    }
}

layout(local_size_x = SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint i = gl_LocalInvocationID.x;
    uvec2 coord = gl_GlobalInvocationID.xy;

    vec3 color = texelFetch(in_tex, ivec3(coord, 0), 0).xyz;

    values[i][0] = vec2[3](vec2(color.r, 0.), vec2(color.g, 0.), vec2(color.b, 0.));
    synchronize();

    int source = 0;

    for (int n = 2; n <= SIZE; n *= 2) {
        fft_pass(n, source);
        source ^= 1;
        synchronize();
    }

    float r = length(values[i][source][0]);
    float g = length(values[i][source][1]);
    float b = length(values[i][source][2]);
    vec4 dft = vec4(r, g, b, 1.);
    dft.rgb /= SIZE;

    // Shift to center
    coord.x = (coord.x + SIZE / 2) % SIZE;
    imageStore(out_tex, ivec2(coord), dft);
}