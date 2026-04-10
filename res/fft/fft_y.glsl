#version 460

layout(rgba32f, binding = 0) uniform image2DArray in_tex;
layout(rgba32f, binding = 1) uniform image2DArray out_tex;

//! option "size"
const uint SIZE = 1024;
//! option "complex"
const bool COMPLEX = true;

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
    uint i = gl_LocalInvocationID.y;

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

layout(local_size_x = 1, local_size_y = SIZE, local_size_z = 1) in;
void main()
{
    uint i = gl_LocalInvocationID.y;
    uvec2 coord = gl_GlobalInvocationID.xy;

    vec3 real = imageLoad(in_tex, ivec3(coord, 0)).xyz;
    vec3 imag = vec3(0);
    if (COMPLEX) {
        imag = imageLoad(in_tex, ivec3(coord, 1)).xyz;
    }

    values[i][0] = vec2[3](vec2(real.r, imag.r), vec2(real.g, imag.g), vec2(real.b, imag.b));
    synchronize();

    int source = 0;

    for (int n = 2; n <= SIZE; n *= 2) {
        fft_pass(n, source);
        source ^= 1;
        synchronize();
    }

    vec4 real_out;
    real_out.r = (values[i][source][0])[0];
    real_out.g = (values[i][source][1])[0];
    real_out.b = (values[i][source][2])[0];
    real_out.a = 1;
    // real_out.rgb /= SIZE;
    vec4 imag_out;
    imag_out.r = (values[i][source][0])[1];
    imag_out.g = (values[i][source][1])[1];
    imag_out.b = (values[i][source][2])[1];
    imag_out.a = 1;
    // imag_out.rgb /= SIZE;

    // Shift to center
    // coord.y = (coord.y + SIZE / 2) % SIZE;
    imageStore(out_tex, ivec3(coord, 0), real_out);
    imageStore(out_tex, ivec3(coord, 1), imag_out);
}