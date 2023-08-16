#version 460

layout(location = 0) out vec4 out_col1;
layout(location = 1) out vec4 out_col2;
layout(location = 2) out vec4 out_col3;
uniform sampler2DArray tex;

uniform vec2 mxy;
uniform ivec2 mlr;
uniform uint mode;
uniform uint cmd;
uniform float zoom;
uniform float scale;

uniform vec3 paint_color;
uniform vec3 paint_mask;
uniform int paint_size;
uniform int paint_layer;
uniform bool paint_smooth;

//	----    ----    ----    ----    ----    ----    ----    ----
//! texture_format GL_RGBA32F

//! option "Max radius" (2, 16)
const uint MAX_RADIUS = 4u;
//! option "Pull radius" (1, 16)
const uint PULL_RAD_IN = 4u;
const uint PULL_RAD = min(MAX_RADIUS, PULL_RAD_IN);
const uint PUSH_RAD = 0u;
const float pull_scale = 5.;
const float push_scale = 1.;

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

	vec4 res_c = gdv(ivec2(0, 0), tex, 0);
	vec4 demand = gdv(ivec2(0, 0), tex, 1);
	vec4 total_demand = gdv(ivec2(0, 0), tex, 2);

	if(mlr.x != 0u) {
		if (paint_layer == 0) {
			res_c = place(res_c, vec4(paint_color, 1.), paint_size, mxy); }
		if (paint_layer == 1) {
			vec4 put_demand = min(vec4(paint_color, 1.), 1. - res_c);
			demand = place(demand, put_demand/area, paint_size, mxy); }
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
