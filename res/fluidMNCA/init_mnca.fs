#version 460

layout(location = 0) out vec4 out_col1;
layout(location = 1) out vec4 out_col2;
layout(location = 2) out vec4 out_col3;
uniform float texture_width;
uniform float texture_height;

uniform uint rand_seed;
uint frames = 0;

//! option "R Power" (0, 1)
uniform float power_r = 0.2;
//! option "G Power" (0, 1)
uniform float power_g = 0.2;
//! option "B Power" (0, 1)
uniform float power_b = 0.2;

//! option "R Scale" exp (0, 10)
uniform float scale_r = 1;
//! option "G Scale" exp (0, 10)
uniform float scale_g = 1;
//! option "B Scale" exp (0, 10)
uniform float scale_b = 1;

//! option "R Amplitude" (0, 1)
uniform float amp_r = 0.4;
//! option "G Amplitude" (0, 1)
uniform float amp_g = 0.4;
//! option "B Amplitude" (0, 1)
uniform float amp_b = 0.4;

//	----    ----    ----    ----    ----    ----    ----    ----

float lmap() {
	return (float(gl_FragCoord[0]) / texture_width);
}
float vmap() {
	return (float(gl_FragCoord[1]) / texture_height);
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

void main() {
    out_col1[0] = power_r * reseed(rand_seed + 0u, scale_r, amp_r) * (1.-lmap());
    out_col1[1] = power_g * reseed(rand_seed + 1u, scale_g, amp_g) * lmap();
    out_col1[2] = power_b * reseed(rand_seed + 2u, scale_b, amp_b) * vmap();
    out_col1[3] = 1.;
}
