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

// Define states
#define GET_DEMAND 0u
#define PUSH_DEMAND 3u
#define GET_TOTAL_DEMAND 1u
#define CALC_FLOW 2u

//	----    ----    ----    ----    ----    ----    ----    ----

//! option "Max radius" (2, 16)
const uint MAX_RADIUS = 4u;
//! option "Pull radius" (1, 16)
const uint PULL_RAD_IN = 4u;
const uint PULL_RAD = min(MAX_RADIUS, PULL_RAD_IN);
const float pull_scale = 5.;

//! option "Restrict value"
const bool RESTRICT_VALUES = false;
//! option "Rule scale" exp (0.1, 2000)
uniform float rule_scale = 70;

//! option "Paint size" (1,128)
uniform int paint_size = 1;
//! option "Paint smooth"
uniform bool paint_smooth = true;
//! option "Paint layer" (0,1)
uniform int paint_layer = 0;
//! option "Paint amplitude" exp (1,1000)
uniform int paint_amp = 1;

//! option "Paint red" (0,1)
uniform float paint_r = 1;
//! option "Paint green" (0,1)
uniform float paint_g = 1;
//! option "Paint blue" (0,1)
uniform float paint_b = 1;

vec3 paint_color = vec3(paint_r, paint_g, paint_b) * paint_amp;

//! option "Paint use red"
uniform bool paint_mask_r = true;
//! option "Paint use green"
uniform bool paint_mask_g = true;
//! option "Paint use blue"
uniform bool paint_mask_b = true;

vec3 paint_mask = vec3(paint_mask_r ? 1 : 0,
					   paint_mask_g ? 1 : 0,
					   paint_mask_b ? 1 : 0);

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
	return float(u32_upk(v, w, w * o));
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

vec4 get_flow(ivec2 pos_offset, sampler2DArray tx, vec4 local_demand) {
	vec4 supply = gdv( pos_offset, tx, 0);
	vec4 total_demand = gdv( pos_offset, tx, 2);
	vec4 used_supply = min(supply, total_demand);
	// used_supply = min(used_supply, vec4(1.));
	vec4 current_flow;
	for (uint i = 0u; i < 4; i++) {
		current_flow[i] = total_demand[i] == 0? 0 : (local_demand[i] / total_demand[i]) * used_supply[i];
	}
	// vec4 current_flow = (1. - step(total_demand, vec4(0.))) * (local_demand / total_demand) * used_supply;
	current_flow = min(current_flow, local_demand);
	return current_flow; }

struct ConvData {
	vec4 	value;
	float 	total;
};
ConvData nbhd( vec2 r, sampler2DArray tx, int layer ) {
	float	psn = 65536.0;
	vec4	value = vec4(0);
	float 	total = 0;
	vec4 local_demand  = gdv( ivec2( 0, 0), tx, 1);

	vec2 r2 = ceil(r + vec2(0.5)) - vec2(0.500001);
	r2 = r2 * r2;
	for(float j = 0.0; j <= r[0]; j++) {
		vec2 bound = sqrt(max(vec2(0),r2 - vec2(j*j)));
		for(float i = floor(bound[1])+1; i <= bound[0]; i++) {
			if (stage == CALC_FLOW) {
				total += 4.0;
				value += get_flow( ivec2( i, j), tx, local_demand );
				value += get_flow( ivec2( j,-i), tx, local_demand );
				value += get_flow( ivec2(-i,-j), tx, local_demand );
				value += get_flow( ivec2(-j, i), tx, local_demand );
			} else {
				total += 4.0;
				value += gdv( ivec2( i, j), tx, layer );
				value += gdv( ivec2( j,-i), tx, layer );
				value += gdv( ivec2(-i,-j), tx, layer );
				value += gdv( ivec2(-j, i), tx, layer );
			} } } 
	return ConvData(value, total); }

vec4 bitring(ConvData[MAX_RADIUS] rings, uint bits, uint of) {
	vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);
	float tot = 0.;
	// for(uint i = 0u; i < MAX_RADIUS; i++) {
	// 	if(u32_upk(bits, 1u, i + of) == 1u) {
	// for(uint i = 0u; i < MAX_RADIUS; i++) {
	// 	if(u32_upk(bits, 1u, i + of) == 1u) {
		uint i = u32_upk(bits, 4u, 0) % MAX_RADIUS;
			sum += rings[i].value;
			tot += rings[i].total;
		// }
	// }
	return sum / tot;
} // TODO

//	----    ----    ----    ----    ----    ----    ----    ----

//	Used to reseed the surface with lumpy noise
//	TODO - Breaks down at 2048+ resolution
float get_xc(float x, float y, float xmod) {
	float sq = sqrt(mod(x * y + y, xmod)) / sqrt(xmod);
	float xc = mod((x * x) + (y * y), xmod) / xmod;
	return clamp((sq + xc) * 0.5, 0.0, 1.0);
}
float shuffle(float x, float y, float xmod, float val) {
	val = val * mod(x * y + x, xmod);
	return (val - floor(val));
}
float get_xcn(float x, float y, float xm0, float xm1, float ox, float oy) {
	float xc = get_xc(x + ox, y + oy, xm0);
	return shuffle(x + ox, y + oy, xm1, xc);
}
float get_lump(float x, float y, float nhsz, float xm0, float xm1) {
	float nhsz_c = 0.0;
	float xcn = 0.0;
	float nh_val = 0.0;
	for(float i = -nhsz; i <= nhsz; i += 1.0) {
		for(float j = -nhsz; j <= nhsz; j += 1.0) {
			nh_val = round(sqrt(i * i + j * j));
			if(nh_val <= nhsz) {
				xcn = xcn + get_xcn(x, y, xm0, xm1, i, j);
				nhsz_c = nhsz_c + 1.0;
			}
		}
	}
	float xcnf = (xcn / nhsz_c);
	float xcaf = xcnf;
	for(float i = 0.0; i <= nhsz; i += 1.0) {
		xcaf = clamp((xcnf * xcaf + xcnf * xcaf) * (xcnf + xcnf), 0.0, 1.0);
	}
	return xcaf;
}
float reseed(uint seed, float scl, float amp) {
	float fx = gl_FragCoord[0];
	float fy = gl_FragCoord[1];
	float r0 = get_lump(fx, fy, round(6.0 * scl), 19.0 + mod(frames + seed, 17), 23.0 + mod(frames + seed, 43));
	float r1 = get_lump(fx, fy, round(22.0 * scl), 13.0 + mod(frames + seed, 29), 17.0 + mod(frames + seed, 31));
	float r2 = get_lump(fx, fy, round(14.0 * scl), 13.0 + mod(frames + seed, 11), 51.0 + mod(frames + seed, 37));
	float r3 = get_lump(fx, fy, round(18.0 * scl), 29.0 + mod(frames + seed, 7), 61.0 + mod(frames + seed, 28));
	return clamp(sqrt((r0 + r1) * r3 * (amp + 1.2)) - r2 * (amp * 1.8 + 0.2), 0.0, 1.0);
}

vec4 place(vec4 col1, vec4 col2, float sz, vec2 mxy) {
	mxy *= textureSize(tex, 0).xy;
	vec2 dxy = (vec2(gl_FragCoord) - mxy) * (vec2(gl_FragCoord) - mxy);
	float dist = sqrt(dxy[0] + dxy[1]);
	float lamda = min(dist / sz, 1.);
	lamda = paint_smooth ? lamda : step(1., lamda);
	col2 = mix(col1, col2, vec4(paint_mask, 1.));
	return mix(col2, col1, lamda);
}

vec4 place(vec4 col, float sz, vec2 mxy, uint s, float off) {
	mxy *= textureSize(tex, 0).xy;
	vec2 dxy = (vec2(gl_FragCoord) - mxy) * (vec2(gl_FragCoord) - mxy);
	float dist = sqrt(dxy[0] + dxy[1]);
	float cy = mod(frames + off, 213.0) / 213.0;
	float c2 = mod(frames + off, 377.0) / 377.0;
	float z2 = ((cos(2.0 * PI * c2) / 2.0) + 0.5);
	float z3 = z2 / 4.0;
	float z4 = z2 - z3;
	float ds = (1.0 - dist / sz);
	float vr = (((cos((1.0 * PI * 4.0 * cy) / 2.0) + 0.5) * z4 + z3) * ds * 0.85 + 0.38 * ds * ds);
	float vg = (((cos((2.0 * PI * 4.0 * cy) / 2.0) + 0.5) * z4 + z3) * ds * 0.85 + 0.38 * ds * ds);
	float vb = (((cos((3.0 * PI * 4.0 * cy) / 2.0) + 0.5) * z4 + z3) * ds * 0.85 + 0.38 * ds * ds);
	if(dist <= sz) {
		col += (s != 1u) ? vec4(-0.38, -0.38, -0.38, -0.38) * ds : vec4(vr, vg, vb, 1.0);
	}
	return col;
}

vec4 mouse(vec4 col, float sz) {
	return place(col, sz, mxy, mlr.x, 0.0);
}

void main() {

//	----    ----    ----    ----    ----    ----    ----    ----
//	Rule Initilisation
//	----    ----    ----    ----    ----    ----    ----    ----

//	NH Rings
	ConvData[MAX_RADIUS] nh_rings_c;
	ConvData nbh = {vec4(0.), 0.};
	if(stage == GET_DEMAND) {
		for(uint i = 0u; i < MAX_RADIUS; i++) {
			nh_rings_c[i] = nbhd(vec2(i + 1u, i), tex, 0);
		}
	} else if(stage == GET_TOTAL_DEMAND) {
		nbh = nbhd(vec2(PULL_RAD, 0), tex, 1);
	} else if (stage == CALC_FLOW) {
		nbh = nbhd(vec2(PULL_RAD, 0), tex, 0);
	}

//	Parameters
	const float mnp = 1.0 / 65536.0;			//	Minimum value of a precise step for 16-bit channel
	const float s = mnp * 48.0 * 64.0;
	const float n = mnp * 48.0 * 2.0;

//	Output Values
	vec4 res_c = gdv(ivec2(0, 0), tex, 0);
	vec4 demand = gdv(ivec2(0, 0), tex, 1);
	vec4 total_demand = gdv(ivec2(0, 0), tex, 2);
	// vec4 push = gdv(ivec2(0, 0), tex, 3);

	float area = 0.;
	if(stage == GET_DEMAND) {
		demand = vec4(0);
		for(uint i = 0u; i < 8u; i++) {
			vec4 nh = bitring(nh_rings_c, nb[i / 8u], (i & 1u) * 16u);

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

			vec4 rr = nh*nnvr*rule_scale;
			vec4 rg = nh*nnvg*rule_scale;
			vec4 rb = nh*nnvb*rule_scale;

			if(i == 0u) {
				demand[0] = (rr[0]+rr[1]+rr[2]);
				demand[1] = (rg[0]+rg[1]+rg[2]);
				demand[2] = (rb[0]+rb[1]+rb[2]);}
			else {
				demand[0] += (rr[0]+rr[1]+rr[2]);
				demand[1] += (rg[0]+rg[1]+rg[2]);
				demand[2] += (rb[0]+rb[1]+rb[2]);}
		}

		// demand *= pull_scale;
		demand = max(demand, vec4(0.));
		if (RESTRICT_VALUES) {
			demand = min(demand, vec4(1.));
			demand = min(demand, 1. - res_c);
		}
		// demand = min(demand, 1. - res_c);
		// Set the PULL AREA
		area = 1.;
		for(uint i = 0u; i < PULL_RAD; i++) {
			area += nh_rings_c[i].total;
		}
		demand /= area;
	}
	if(stage == PUSH_DEMAND) {
		// float area = ((2 * PULL_RAD + 1) * (2 * PULL_RAD + 1));
		// float area = res_v[2];
		// float sum = 0.;
		// sum += nbh.value;
		// res_v[1] += sum;
		// res_v[1] = min(1., res_v[1]);
		// res_v[1] = min(res_v[1], 1. - res_v[0]);
		// res_v[1] = res_v[1] / area;
	}
	if(stage == GET_TOTAL_DEMAND) {
		vec4 sum = demand;
		sum += nbh.value;
		total_demand = sum;
	}
	if(stage == CALC_FLOW) {
		vec4 local_demand = gdv(ivec2(0, 0), tex, 1);
		vec4 sum = get_flow(ivec2(0, 0), tex, local_demand);
		sum += nbh.value;

		res_c -= min(res_c, total_demand);
		res_c += sum;
		// res_c = min(vec4(1.), res_c);
		if (RESTRICT_VALUES) {
			res_c = min(vec4(1.), res_c);
		}
		res_c = max(vec4(0.), res_c);
		area = ((2 * PULL_RAD + 1) * (2 * PULL_RAD + 1));
		demand *= area;
		total_demand = vec4(0.);

		// res_c.rgb = res_c.grb;
	}

//	----    ----    ----    ----    ----    ----    ----    ----
//	Shader Output
//	----    ----    ----    ----    ----    ----    ----    ----

	if( cmd == 2u ) {
		res_c[0] = 0.0; 
		res_c[1] = 0.0; 
		res_c[2] = 0.0; 
		res_c[3] = 1.0; }

	if(mlr.x != 0u) {
		if (paint_layer == 0) {
			res_c = place(res_c, vec4(paint_color, 1.), paint_size, mxy); }
		if (paint_layer == 1 && stage % 3 == GET_DEMAND) {
			vec4 put_demand = vec4(paint_color, 1.);
			if (RESTRICT_VALUES)
				put_demand = min(put_demand, (1. - res_c) / area);
			demand = place(demand, put_demand, paint_size, mxy); }
	}
	if(mlr.y != 0u) {
		res_c = mouse(res_c, paint_size);	}

	//	Force alpha to 1.0
	res_c[3] = 1.0;

	out_col1 = res_c;
	out_col2 = demand;
	out_col3 = total_demand;
	// out_col4 = demand;
}
