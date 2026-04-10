#version 460

layout(rgba32f, binding = 0) uniform image2DArray in_tex;
layout(rgba32f, binding = 1) uniform image2DArray out_tex;

//! option "size"
const uint SIZE = 1024;
//! option "complex_in"
const bool COMPLEX_IN = true;
//! option "complex_out"
const bool COMPLEX_OUT = true;
//! option "inverse"
const bool INVERSE = false;
//! option "horizontal"
const bool HORIZONTAL = true;
//! option "abs"
const bool ABS = false;
//! option "normalization"
const int NORMALIZATION = 1;

#define PI 3.14159265358979323844

shared vec2 values[SIZE][2][3];

void synchronize()
{
    memoryBarrierShared();
    barrier();
}

vec2 cmul(vec2 a, vec2 b)
{
    return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x);
}

void
fft_pass(int ns, int source)
{
    int i = int(HORIZONTAL? gl_LocalInvocationID.x : gl_LocalInvocationID.y);

    float n2 = ns/2.0;

    float base = (i/ns)*n2;
    float offs = mod(i,n2);

    int i0 = int(base + offs);
    int i1 = i0 + int(SIZE/2);
    
    vec2[3] v0 = values[i0][source];
    vec2[3] v1 = values[i1][source];
    
    float a = -2.*PI*float(i%ns)/ns * float(INVERSE ? -1 : 1);

    float t_re = cos(a);
    float t_im = sin(a);

    for (uint channel = 0; channel < 3; channel++) {
        values[i][source ^ 1][channel] = v0[channel] + cmul(vec2(t_re, t_im), v1[channel]); 
    }
}

const uint SIZE_X = HORIZONTAL ? SIZE : 1;
const uint SIZE_Y = HORIZONTAL ? 1 : SIZE;
layout(local_size_x = SIZE_X, local_size_y = SIZE_Y, local_size_z = 1) in;
void main()
{
    uint i = HORIZONTAL? gl_LocalInvocationID.x : gl_LocalInvocationID.y;
    uvec3 coord = gl_GlobalInvocationID.xyz;

    vec3 real = vec3(0); 
    vec3 imag = vec3(0);
    if (COMPLEX_IN) {
        real = imageLoad(in_tex, ivec3(coord.xy, coord.z * 2)).xyz;
        imag = imageLoad(in_tex, ivec3(coord.xy, coord.z * 2 + 1)).xyz;
    } else {
        real = imageLoad(in_tex, ivec3(coord.xy, coord.z)).xyz;
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
    real_out.rgb /= NORMALIZATION;
    vec4 imag_out;
    imag_out.r = (values[i][source][0])[1];
    imag_out.g = (values[i][source][1])[1];
    imag_out.b = (values[i][source][2])[1];
    imag_out.a = 1;
    imag_out.rgb /= NORMALIZATION;

    // Shift to center
    // coord.x = (coord.x + SIZE / 2) % SIZE;
    if (COMPLEX_OUT) {
        imageStore(out_tex, ivec3(coord.xy, coord.z * 2), real_out);
        imageStore(out_tex, ivec3(coord.xy, coord.z * 2 + 1), imag_out);
    } else {
        imageStore(out_tex, ivec3(coord.xy, coord.z), real_out);
    }

    if (ABS) {
        vec4 abs_val = sqrt(real_out * real_out + imag_out * imag_out);
        imageStore(out_tex, ivec3(coord.xy, coord.z), abs_val);
    }
}