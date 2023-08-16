#version 460
#define PI 3.14159265359
#define LN 2.71828182846

//	----    ----    ----    ----    ----    ----    ----    ----

out vec4 out_col;
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

uniform uint v63;
uniform uint frames;

//	----    ----    ----    ----    ----    ----    ----    ----

//! option "Max radius" (2, 16)
const uint MAX_RADIUS = 12u;

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

vec4 gdv(ivec2 of, sampler2DArray tx) {
	of = ivec2(gl_FragCoord) + of;
	of = ivec2(mod(of, textureSize(tx, 0).xy));
	return texelFetch(tx, ivec3(of, 0.), 0);
}

vec4[2] nbhd(vec2 r, sampler2DArray tx) {
	float psn = 65536.0;
	vec4 a = vec4(0);
	vec4 b = vec4(0);
	vec2 r2 = ceil(r + vec2(0.5)) - vec2(0.500001);
	r2 = r2 * r2;
	for(float j = 0.0; j <= r[0]; j++) {
		vec2 bound = sqrt(max(vec2(0), r2 - vec2(j * j)));
		for(float i = floor(bound[1]) + 1; i <= bound[0]; i++) {
			b += psn * 4.0;
			vec4 t0 = gdv(ivec2(i, j), tx) * psn;
			a += t0 - fract(t0);
			vec4 t1 = gdv(ivec2(j, -i), tx) * psn;
			a += t1 - fract(t1);
			vec4 t2 = gdv(ivec2(-i, -j), tx) * psn;
			a += t2 - fract(t2);
			vec4 t3 = gdv(ivec2(-j, i), tx) * psn;
			a += t3 - fract(t3);
		}
	}
	return vec4[2] (a, b);
}

vec4 bitring(vec4[MAX_RADIUS][2] rings, uint bits, uint of) {
	vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 tot = vec4(0.0, 0.0, 0.0, 0.0);
	for(uint i = 0u; i < MAX_RADIUS; i++) {
		if(u32_upk(bits, 1u, i + of) == 1u) {
			sum += rings[i][0];
			tot += rings[i][1];
		}
	}
	return sigm((sum / tot), LN);
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
	vec4[MAX_RADIUS][2] nh_rings_c;
	for(uint i = 0u; i < MAX_RADIUS; i++) {
		nh_rings_c[i] = nbhd(vec2(i + 1u, i), tex);
	}

//	Parameters
	const float mnp = 1.0 / 65536.0;			//	Minimum value of a precise step for 16-bit channel
	const float s = mnp * 48.0 * 64.0;
	const float n = mnp * 48.0 * 2.0;

//	Output Values
	vec4 res_c = gdv(ivec2(0, 0), tex);

//	Result Values
	vec4 res_v = res_c;

	for(uint i = 0u; i < 24u; i++) {
		uint  	cho = u32_upk( ch[i/8u], 2u, (i*4u+0u) & 31u );
				cho = (cho == 3u) ? u32_upk( ch2[i/8u], 2u, (i*4u+0u) & 31u ) : cho;
		uint  	chi = u32_upk( ch[i/8u], 2u, (i*4u+2u) & 31u );
				chi = (chi == 3u) ? u32_upk( ch2[i/8u], 2u, (i*4u+2u) & 31u ) : chi;
		uint  	chm = u32_upk( ch3[i/8u], 2u, (i*4u+2u) & 31u );
				chm = (chm == 3u) ? u32_upk( ch[i/8u], 2u, (i*4u+2u) & 31u ) : chm;

		float nhv = bitring(nh_rings_c, nb[i / 2u], (i & 1u) * 16u)[cho];

		if(nhv >= utp(ur[i], 8u, 0u) && nhv <= utp(ur[i], 8u, 1u)) {
			float h = hmp2(res_c[chm], 1.2);
			res_v[chi] += bsn(us[i / 16u], ((i * 2u + 0u) & 31u)) * s * h;
		}
		if(nhv >= utp(ur[i], 8u, 2u) && nhv <= utp(ur[i], 8u, 3u)) {
			float h = hmp2(res_c[chm], 1.2);
			res_v[chi] += bsn(us[i / 16u], ((i * 2u + 1u) & 31u)) * s * h;
		}
	}

	vec4 n4 = sigm(res_v, 0.5) * n * 64.0 + n;
	res_c = res_v - n4;

//	----    ----    ----    ----    ----    ----    ----    ----
//	Shader Output
//	----    ----    ----    ----    ----    ----    ----    ----

	if(frames <= 0u || cmd == 1u) {
		res_c[0] = reseed(u32_upk(v63, 8u, 24u) + 0u, 1.0, 0.4);
		res_c[1] = reseed(u32_upk(v63, 8u, 24u) + 1u, 1.0, 0.4);
		res_c[2] = reseed(u32_upk(v63, 8u, 24u) + 2u, 1.0, 0.4);
		res_c[3] = reseed(u32_upk(v63, 8u, 24u) + 3u, 1.0, 0.4);
	}

	if( cmd == 2u ) {
		res_c[0] = 0.0; 
		res_c[1] = 0.0; 
		res_c[2] = 0.0; 
		res_c[3] = 1.0; }

	if(mlr.x != 0u) {
		res_c = mouse(res_c, 38.0);	}

	//	Force alpha to 1.0
	res_c[3] = 1.0;

	out_col = res_c;
}