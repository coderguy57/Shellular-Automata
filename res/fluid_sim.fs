#version 460

//	----    ----    ----    ----    ----    ----    ----    ----

out vec4 out_col;
uniform sampler2DArray tex;
uniform uint cmd;

uniform uint frames;

uniform vec2 mxy;
uniform ivec2 mlr;
uniform vec3 paint_color;
uniform vec3 paint_mask;
uniform int paint_size;
uniform bool paint_smooth;

//	----    ----    ----    ----    ----    ----    ----    ----

const vec2 diff = vec2(0.08,0.03); // difusion coefficients of uv
float dt = 1;
float dx = 1;

//	----    ----    ----    ----    ----    ----    ----    ----

float lmap() {
	return (gl_FragCoord[0] / textureSize(tex, 0)[0]);
}
float vmap() {
	return (gl_FragCoord[1] / textureSize(tex, 0)[1]);
}

vec4 gdv(ivec2 of, sampler2DArray tx) {
	of = ivec2(gl_FragCoord) + of;
	of = ivec2(mod(of, textureSize(tx, 0).xy));
	return texelFetch(tx, ivec3(of, 0.), 0);
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

vec4 lap() {
	ivec2 pos = ivec2(gl_FragCoord);

	vec4 res = vec4(0.);
	res += gdv(ivec2(-1., 0.), tex);
	res += gdv(ivec2(+1., 0.), tex);
	res += gdv(ivec2(0., +1.), tex);
	res += gdv(ivec2(0., -1.), tex);
	res -= 4. * gdv(ivec2(0., 0.), tex);
	return res / (dx * dx);
}

void main() {

	vec4 state = gdv(ivec2(0, 0), tex);
	vec2 vel = state.xy;
	float density = state.z;

	vec4 laplace = lap();
	float u = res_c[0];
	float v = res_c[1];

	float F = (1 - lmap()) * 0.05;
	float k = (1 - vmap()) * 0.05 + 0.025;
	
	float du = -u*v*v + F*(1 - u) + diff.x * laplace[0];
	float dv = +u*v*v - v*(F + k) + diff.y * laplace[1];

	res_c[0] += dt * du;
	res_c[1] += dt * dv;
	res_c[2] = 0;
	res_c = max(vec4(0.), min(vec4(1.), res_c));

//	----    ----    ----    ----    ----    ----    ----    ----
//	Shader Output
//	----    ----    ----    ----    ----    ----    ----    ----

	if(frames <= 0u || cmd == 1u) {
		// res_c[0] = reseed(u32_upk(v63, 8u, 24u) + 0u, 1.0, 0.4);
		// res_c[1] = reseed(u32_upk(v63, 8u, 24u) + 1u, 1.0, 0.4);
		// res_c[2] = reseed(u32_upk(v63, 8u, 24u) + 2u, 1.0, 0.4);
		// res_c[3] = reseed(u32_upk(v63, 8u, 24u) + 3u, 1.0, 0.4);
	}

	if(mlr.x != 0u) {
		res_c = place(res_c, vec4(paint_color, 1.), paint_size, mxy);
	}

	if( cmd == 2u ) {
		res_c[0] = 0.0; 
		res_c[1] = 0.0; 
		res_c[2] = 0.0; 
		res_c[3] = 1.0; }

	//	Force alpha to 1.0
	res_c[3] = 1.0;

	out_col = res_c;
}