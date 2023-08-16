#version 460
#define PI 3.14159265359
#define LN 2.71828182846

layout(location = 0) out vec4 out_col1;
layout(location = 1) out vec4 out_col2;
layout(location = 2) out vec4 out_col3;
// layout(location = 3) out vec4 out_col4;
uniform sampler2DArray tex;
uniform uint[12] nb;
uniform uint[24] ur;
uniform uint[2] us; //= uint[2] (ub.v36, ub.v37);
uniform uint[3] ch; //= uint[3] (ub.v38, ub.v39, ub.v40);
uniform uint[3] ch2; // = uint[3] (2286157824u, 295261525u, 1713547946u);
uniform uint[3] ch3; //= uint[3] (ub.v41, ub.v42, ub.v43);
uniform vec2 mxy;
uniform ivec2 mlr;
uniform uint mode;
uniform uint cmd;
uniform float zoom;
uniform float scale;
uniform uint stage;

uniform uint rand_seed;
uniform uint frames;

uniform vec3 paint_color;
uniform vec3 paint_mask;
uniform int paint_size;
uniform int paint_layer;
uniform bool paint_smooth;

// Define states
#define GET_DEMAND 0u
#define GET_TOTAL_DEMAND 1u
#define CALC_FLOW 2u

//! option "Negative factor" exp (0, 5)
uniform float test_factor = 0;
//! option "Overlap" exp (-1, 1)
uniform float overlap = 0;

//! option "H func"
uniform bool hfunc = true;

//! option "Sharp corners"
const bool sharp = true;
//! option "Smooth corner factor 1" exp (0.1, 100)
uniform float exp_factor = 1;
//! option "Smooth corner factor 2" exp (0.1, 200)
uniform float exp_factor_2 = 1;

//! option "Number of steps" (1, 24)
const uint MAX_STEPS = 24;

//! option "MNCAS STEPS" (0, 6)
const uint MNCAS_INDEX = 3;
const uint[7] MNCA_SELECTIVE = uint[7](1,2,3,4,6,12,24);
const uint MNCAS_STEPS = MNCA_SELECTIVE[MNCAS_INDEX];



//	----    ----    ----    ----    ----    ----    ----    ----
//! texture_format GL_RGBA32F

//! option "Max radius" (2, 16)
const uint MAX_RADIUS = 10u;
//! option "Pull radius" (1, 16)
const uint PULL_RAD_IN = 4u;
const uint PULL_RAD = min(MAX_RADIUS, PULL_RAD_IN);
const float pull_scale = 5.;

//! option "Rule scale" exp (0.1, 200)
uniform float rule_scale = 1;

//	----    ----    ----    ----    ----    ----    ----    ----

uint u32_upk(uint u32, uint bts, uint off) {
	return (u32 >> off) & ((1u << bts) - 1u);
}

float lmap() {
	return (gl_FragCoord[0] / textureSize(tex, 0)[0]);
}
float vmap() {
	return (gl_FragCoord[1] / textureSize(tex, 0)[1]);
}
float cmap() {
	return sqrt(((gl_FragCoord[0] - textureSize(tex, 0)[0] * 0.5) / textureSize(tex, 0)[0] * 0.5) * ((gl_FragCoord[0] - textureSize(tex, 0)[0] * 0.5) / textureSize(tex, 0)[0] * 0.5) + ((gl_FragCoord[1] - textureSize(tex, 0)[1] * 0.5) / textureSize(tex, 0)[1] * 0.5) * ((gl_FragCoord[1] - textureSize(tex, 0)[1] * 0.5) / textureSize(tex, 0)[1] * 0.5));
}

float vwm() {
	float scale_raw = scale;
	float scale_new = scale_raw;
	if(mode == 1u) { //	Linear Parameter Map
		scale_new = ((lmap() + zoom) * (scale_raw / (1.0 + zoom * 2.0))) * 2.0;
	}
	if(mode == 2u) { //	Circular Parameter Map
		scale_new = ((sqrt(cmap()) + zoom) * (scale_raw / (1.0 + zoom * 2.0))) * 2.0;
	}
	return scale_new;
}

float tp(uint n, float s) {
	return (float(n + 1u) / 256.0) * ((s * 0.5) / 128.0);
}
float bsn(uint v, uint o) {
	return float(u32_upk(v, 1u, o) * 2u) - 1.0;
}
float utp(uint v, uint w, uint o) {
	return tp(u32_upk(v, w, w * o), vwm());
}

vec4 sigm(vec4 x, float w) {
	return 1.0 / (1.0 + exp((-w * 2.0 * x * (PI / 2.0)) + w * (PI / 2.0)));
}
float hmp2(float x, float w) {
	return 3.0 * ((x - 0.5) * (x - 0.5)) + 0.25;
}

vec4 gdv(ivec2 of, sampler2DArray tx, int layer) {
	of = ivec2(gl_FragCoord) + of;
	of = ivec2(mod(of, textureSize(tx, 0).xy));
	return texelFetch(tx, ivec3(of, layer), 0);
}


vec4[MAX_RADIUS][3] nh_rings_c;
void dostuff(uint i) {
	const float psn = 65536.0;
	const uint rad = MAX_RADIUS;
	const uint size = rad * 2 + 1;
	const uint x = i % size; 
	const uint y = i / size; 
	const int xPos = int(x - rad);
	const int yPos = int(y - rad);
	const uint lenght = uint(round(sqrt((xPos * xPos) + (yPos * yPos))));
	if (lenght < rad) {
		const ivec2 pixCoord = ivec2(xPos, yPos);
		nh_rings_c[lenght][0] += gdv(pixCoord, tex, 0) * (float(xPos) / lenght);
		nh_rings_c[lenght][1] += gdv(pixCoord, tex, 0) * (float(yPos) / lenght);
		nh_rings_c[lenght][2] += float(xPos) / lenght;
	}
}
void test() {
	for(uint i = 0u; i < MAX_RADIUS; i++) {
		nh_rings_c[i][0] = vec4(0, 0, 0, 0);
		nh_rings_c[i][1] = vec4(0, 0, 0, 0);
	}
	const uint rad = MAX_RADIUS;
	const uint size = rad * 2 + 1;
	//! UNROLL
	for (uint i = 0; i < (MAX_RADIUS * 2 + 1) * (MAX_RADIUS * 2 + 1); i++) {
		dostuff(i);
	}
}

vec4[2] bitring(vec4[MAX_RADIUS][3] rings, uint bits, uint of) {
	vec4 sum_x = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 sum_y = vec4(0.0, 0.0, 0.0, 0.0);
	float tot = 0.;
	for(uint i = 0u; i < MAX_RADIUS; i++) {
		if(u32_upk(bits, 1u, i + of) == 1u) {
			sum_x += rings[i][0];
			sum_y += rings[i][1];
			tot += rings[i][2][0];
		}
	}
	return vec4[2](sum_x / tot, sum_y / tot);
}

void main() {

//	----    ----    ----    ----    ----    ----    ----    ----
//	Rule Initilisation
//	----    ----    ----    ----    ----    ----    ----    ----

//	NH Rings
	// ConvData[MAX_RADIUS] nh_rings_c;
	// ConvData nbh = {vec4(0.), 0.};
	// for(uint i = 0u; i < MAX_RADIUS; i++) {
	// 	nh_rings_c[i] = nbhd(vec2(i + 1u, i), tex, 0);
	// }
	test();

//	Parameters
	const float mnp = 1.0 / 65536.0;			//	Minimum value of a precise step for 16-bit channel
	const float s = mnp * 48.0 * 64.0;
	const float n = mnp * 48.0 * 2.0;

//	Output Values
	vec4 res_c = gdv(ivec2(0, 0), tex, 0);
	vec4 x_force = vec4(0.);
	vec4 y_force = vec4(0.);
	// vec4 push = gdv(ivec2(0, 0), tex, 3);

	for(uint i = 0u; i < 8u; i++) {
		vec4[2] nh = bitring(nh_rings_c, nb[i / 8u], (i & 1u) * 16u);

		vec4 nnvr = vec4(
			utp(ur[3 * i + 0], 8u, 0u) / 128. - 1,
			utp(ur[3 * i + 0], 8u, 1u) / 128. - 1,
			utp(ur[3 * i + 0], 8u, 2u) / 128. - 1,
			1.0
		);

		vec4 nnvg = vec4(
			utp(ur[3 * i + 1], 8u, 0u) / 128. - 1,
			utp(ur[3 * i + 1], 8u, 1u) / 128. - 1,
			utp(ur[3 * i + 1], 8u, 2u) / 128. - 1,
			1.0
		);

		vec4 nnvb = vec4(
			utp(ur[3 * i + 2], 8u, 0u) / 128. - 1,
			utp(ur[3 * i + 2], 8u, 1u) / 128. - 1,
			utp(ur[3 * i + 2], 8u, 2u) / 128. - 1,
			1.0
		);

		vec4 rr_x = nh[0]*nnvr*rule_scale;
		vec4 rg_x = nh[0]*nnvg*rule_scale;
		vec4 rb_x = nh[0]*nnvb*rule_scale;

		vec4 rr_y = nh[1]*nnvr*rule_scale;
		vec4 rg_y = nh[1]*nnvg*rule_scale;
		vec4 rb_y = nh[1]*nnvb*rule_scale;

		if(i == 0u) {
			x_force[0] = (rr_x[0]+rr_x[1]+rr_x[2]);
			x_force[1] = (rg_x[0]+rg_x[1]+rg_x[2]);
			x_force[2] = (rb_x[0]+rb_x[1]+rb_x[2]);
			y_force[0] = (rr_y[0]+rr_y[1]+rr_y[2]);
			y_force[1] = (rg_y[0]+rg_y[1]+rg_y[2]);
			y_force[2] = (rb_y[0]+rb_y[1]+rb_y[2]); }
		else {
			x_force[0] += (rr_x[0]+rr_x[1]+rr_x[2]);
			x_force[1] += (rg_x[0]+rg_x[1]+rg_x[2]);
			x_force[2] += (rb_x[0]+rb_x[1]+rb_x[2]);
			y_force[0] += (rr_y[0]+rr_y[1]+rr_y[2]);
			y_force[1] += (rg_y[0]+rg_y[1]+rg_y[2]);
			y_force[2] += (rb_y[0]+rb_y[1]+rb_y[2]); }
	}

//	----    ----    ----    ----    ----    ----    ----    ----
//	Shader Output
//	----    ----    ----    ----    ----    ----    ----    ----

	if( cmd == 2u ) {
		res_c[0] = 0.0; 
		res_c[1] = 0.0; 
		res_c[2] = 0.0; 
		res_c[3] = 1.0; }

	// if(mlr.x != 0u) {
	// 	if (paint_layer == 0) {
	// 		res_c = place(res_c, vec4(paint_color, 1.), paint_size, mxy); }
	// 	if (paint_layer == 1 && stage % 3 == GET_DEMAND) {
	// 		vec4 put_demand = min(vec4(paint_color, 1.), 1. - res_c);
	// 		demand = place(demand, put_demand/area, paint_size, mxy); }
	// }
	// if(mlr.y != 0u) {
	// 	res_c = mouse(res_c, paint_size);	}

	// test();
	// for(uint i = 0u; i < MAX_RADIUS; i++) {
	// 	res_c += test_rings[i] * 0.00001;
	// }
	//	Force alpha to 1.0
	res_c[3] = 1.0;

	res_c = min(vec4(1), res_c);

	out_col1 = res_c;
	out_col2 = x_force;
	out_col3 = y_force;
	// out_col4 = demand;
}
