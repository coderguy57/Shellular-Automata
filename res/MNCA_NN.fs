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
	// return tp(u32_upk(v, w, w * o), vwm());
	return u32_upk(v, w, w * o);
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

vec4[MAX_RADIUS][2] nh_rings_c;
void dostuff(uint i) {
	const float psn = 65536.0;
	const uint rad = MAX_RADIUS;
	const uint size = rad * 2 + 1;
	const uint x = i % size; 
	const uint y = i / size; 
	const int xPos = int(x - rad);
	const int yPos = int(y - rad);
	const uint lenght = uint(sqrt((xPos * xPos) + (yPos * yPos)) + 0.5);
	if (lenght < rad) {
		const ivec2 pixCoord = ivec2(xPos, yPos);
		vec4 temp = gdv(pixCoord, tex) * psn;
		nh_rings_c[lenght][0] += temp - fract(temp);
		nh_rings_c[lenght][1] += psn;
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
	for (uint i = 0; i < size * size; i++) {
		dostuff(i);
	}
}

vec4 bitring(vec4[MAX_RADIUS][2] rings, uint bits, uint of) {
	vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 tot = vec4(0.0, 0.0, 0.0, 0.0);
	// for(uint i = 0u; i < MAX_RADIUS; i++) {
	// 	if(u32_upk(bits, 1u, i + of) == 1u) {
		uint i = u32_upk(bits, 4u, 0);
		sum += rings[i][0];
		tot += rings[i][1];
	// 	}
	// }
	return sum / tot;
}

void main() {

//	----    ----    ----    ----    ----    ----    ----    ----
//	Rule Initilisation
//	----    ----    ----    ----    ----    ----    ----    ----

//	NH Rings
	test();

//	Parameters
	const float mnp = 1.0 / 65536.0;			//	Minimum value of a precise step for 16-bit channel
	const float s = mnp * 48.0 * 64.0;
	const float n = mnp * 48.0 * 2.0;

//	Output Values
	vec4 res_c = gdv(ivec2(0, 0), tex);

//	Result Values
	vec4 res_v = res_c;

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

	vec4 rr = nh*nnvr*scale;
	vec4 rg = nh*nnvg*scale;
	vec4 rb = nh*nnvb*scale;

	if(i == 0u) {
		res_c[0] = (rr[0]+rr[1]+rr[2]);
		res_c[1] = (rg[0]+rg[1]+rg[2]);
		res_c[2] = (rb[0]+rb[1]+rb[2]);}
	else {
		res_c[0] += (rr[0]+rr[1]+rr[2]);
		res_c[1] += (rg[0]+rg[1]+rg[2]);
		res_c[2] += (rb[0]+rb[1]+rb[2]);}
}

	// vec4 n4 = sigm(res_v, 0.5) * n * 64.0 + n;
	// res_c = res_v - n4;

//	----    ----    ----    ----    ----    ----    ----    ----
//	Shader Output
//	----    ----    ----    ----    ----    ----    ----    ----

	if( cmd == 2u ) {
		res_c[0] = 0.0; 
		res_c[1] = 0.0; 
		res_c[2] = 0.0; 
		res_c[3] = 1.0; }

	//	Force alpha to 1.0
	res_c = min(vec4(1.), max(vec4(0.), res_c));
	res_c[3] = 1.0;

	out_col = res_c;
}