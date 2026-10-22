#version 460

layout(rgba32f, binding = 0) uniform image2DArray demand;
layout(rgba32f, binding = 1) uniform image2DArray dataIn;
layout(rgba32f, binding = 2) uniform image2DArray dataOut;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform float demand_scale = 0.01;

float get_demand(int x, int y) {
    ivec3 image_size = imageSize(demand);

    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz) + ivec3(x, y, 0);
    coord = (coord % image_size + image_size) % image_size;
    return imageLoad(demand, coord).z;
}

float xDiff() {
    float colul = -1*get_demand(-1, -1);
    float colml = -2*get_demand(-1, 0);
    float coldl = -1*get_demand(-1, 1);

    float colur = +1*get_demand(1, -1);
    float colmr = +2*get_demand(1, 0);
    float coldr = +1*get_demand(1, 1);

    float res = colul + colml + coldl + colur + colmr + coldr;
    return res / 8;
}

float yDiff() {
    float colul = -1*get_demand(-1, -1);
    float colum = -2*get_demand(0, -1);
    float colur = -1*get_demand(1, -1);

    float coldl = +1*get_demand(-1, 1);
    float coldm = +2*get_demand(0, 1);
    float coldr = +1*get_demand(1, 1);

    float res = colul + colum + colur + coldl + coldm + coldr;
    return res / 8;
}

void main() {
    float x = xDiff();
    float y = yDiff();
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    vec4 res = imageLoad(dataIn, coord);

    res.x += x * demand_scale;
    res.y += y * demand_scale;

    imageStore(dataOut, coord, res);
}
