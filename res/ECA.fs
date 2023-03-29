#version 460
#define PI 3.14159265359
#define LN 2.71828182846

layout(location = 0) out vec4 out_col1;
uniform sampler2DArray tex;
uniform uint frames;
uniform uint cmd;
uniform uint stage;

//	----    ----    ----    ----    ----    ----    ----    ----

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 gdv(ivec2 of, sampler2DArray tx, int layer) {
	of = ivec2(gl_FragCoord) + of;
	of = ivec2(mod(of, textureSize(tx, 0).xy));
	return texelFetch(tx, ivec3(of, layer), 0);
}

ivec2 get_pos() {
	return ivec2(gl_FragCoord);
}

ivec2 get_origin() {
	ivec2 size = ivec2(textureSize(tex, 0).xy);
	ivec2 origin = size / 2;
	origin.y = 0;
	return origin;
}

float get_prev(ivec2 pos) {
	float m = gdv(pos, tex, 0).x;
	float prev = gdv(pos, tex, 0).y;
	return prev; // > 0.99*m ? 1 : 0; // Hack to make the prev value not noticable
}

vec2 swap() {
	float m = gdv(ivec2(0, 0), tex, 0).x;
	float prev = get_prev(ivec2(0, 0));
	return vec2(prev, m);
}

float operation(int rule, float l, float m, float r) {
	int local = (int(l) << 2) + (int(m) << 1) + int(r);
	int sel = 1 << local;
	if ((rule & sel) != 0) {
		m = 1;
	} else {
		m = 0;
	}
	return m;
}

vec2 operation(int rule, float l, float m, float r, float prev) {
	float curr = m;

	int local = (int(l) << 2) + (int(m) << 1) + int(r);
	int sel = 1 << local;
	if ((rule & sel) != 0) {
		m = 1 - prev;
	} else {
		m = prev;
	}
	prev = curr;
	return vec2(m, prev);
}

vec2 do_rule_forward(int rule, ivec2 offset) {
	float l = gdv(ivec2(-1, 0)+offset, tex, 0).x;
	float m = gdv(ivec2(+0, 0)+offset, tex, 0).x;
	float r = gdv(ivec2(+1, 0)+offset, tex, 0).x;

	return vec2(operation(rule, l, m, r));
}

vec2 do_rule(int rule) {
	float l = gdv(ivec2(-1, 0), tex, 0).x;
	float m = gdv(ivec2(0, 0), tex, 0).x;
	float r = gdv(ivec2(1, 0), tex, 0).x;

	float prev = get_prev(ivec2(0, 0));

	return operation(rule, l, m, r, prev);
}

vec2 do_inverse(int rule) {
	float l = get_prev(ivec2(-1, 0));
	float m = get_prev(ivec2(0, 0));
	float r = get_prev(ivec2(1, 0));
	float prev = gdv(ivec2(0, 0), tex, 0).x;

	vec2 state = operation(rule, l, m, r, prev);
	return state.yx;
}

vec2 init_rule(int rule) {
	ivec2 pos = get_pos();
	ivec2 origin = get_origin();


	if (pos.y == origin.y) {
		float m = gdv(ivec2(0, 0), tex, 0).x;
		float prev = get_prev(ivec2(0, 0));
		// float prev = gdv(ivec2(0, 0), tex, 0).y;
		// prev = prev > 0.99*m ? 1 : 0; // Hack to make the prev value not noticable

		return vec2(m, prev);
	} else {
		float above = stage % 2 == 0? -1 : 0;
		vec2 state;
		// if (stage == 2) {
		// 	float l = get_prev(ivec2(-1, 0));
		// 	float m = get_prev(ivec2(0, 0));
		// 	float r = get_prev(ivec2(1, 0));
		// 	float prev = gdv(ivec2(0, 0), tex, 0).y;
		// 	state = operation(rule, l, m, r, prev);
		// 	state.xy = state.yx;
		// } else {
		// 	float l = gdv(ivec2(-1, above), tex, 0).x;
		// 	float m = gdv(ivec2(0, above), tex, 0).x;
		// 	float r = gdv(ivec2(1, above), tex, 0).x;
		// 	float prev = get_prev(ivec2(0, above));
		// 	state = operation(rule, l, m, r, prev);
		// }
		if (frames < 256)
			above = -1;
		else
			above = frames % 2 == 1 ? 0 : 0;
		state = swap().yx;
		float l = gdv(ivec2(-1, above), tex, 0).x;
		float m = gdv(ivec2(0, above), tex, 0).x;
		float r = gdv(ivec2(1, above), tex, 0).x;
		float prev = get_prev(ivec2(0, above));
		if (frames < 256) {
			state = operation(rule, l, m, r, prev);
		} else if (pos.y > (frames - 257) / 2) {
			state = operation(rule, l, m, r, prev);
		}
		return state;
	}
}

vec2 init_rule_forward(int rule) {
	ivec2 pos = get_pos();
	ivec2 origin = get_origin();


	if (pos.y == origin.y) {
		float m = gdv(ivec2(0, 0), tex, 0).x;
		return vec2(m, m);
	} else {
		float above = -1;
		float l = gdv(ivec2(-1, above), tex, 0).x;
		float m = gdv(ivec2(0, above), tex, 0).x;
		float r = gdv(ivec2(1, above), tex, 0).x;
		m = operation(rule, l, m, r);
		return vec2(m, m);
	}
}

vec2 init_rule_forward2(int rule) {
	ivec2 pos = get_pos();
	ivec2 origin = get_origin();


	if (frames - 256 <= pos.y) {
		float m = gdv(ivec2(0, 0), tex, 0).x;
		return vec2(m, m);
	} else {
		float above = 0;
		float l = gdv(ivec2(-1, above), tex, 0).x;
		float m = gdv(ivec2(0, above), tex, 0).x;
		float r = gdv(ivec2(1, above), tex, 0).x;
		m = operation(rule, l, m, r);
		return vec2(m, m);
	}
}


void main() {

//	----    ----    ----    ----    ----    ----    ----    ----
//	Rule Initilisation
//	----    ----    ----    ----    ----    ----    ----    ----
	ivec2 pos = get_pos();
	ivec2 origin = get_origin();

	int rule; // = stage % 2 == 1? 199 : 150;
	int r1 = 243;
	int r2 = 207;
	int r3 = 48;
	// if (stage == 0)
	// 	rule = r1;
	// if (stage == 1)
	// 	rule = r2;
	// if (stage == 2)
	// 	rule = r3;

	if (frames % 3 == 0)
		rule = r1;
	if (frames % 3 == 1)
		rule = r2;
	if (frames % 3 == 2)
		rule = r3;
	
	vec2 state;

	bool random = false;
	r1 = 60;
	r2 = 150;
	if (frames < 256)
		state = init_rule_forward(r1);
	else if (frames < 512)
		state = init_rule_forward2(r2);
	else {
		int rule = frames % 2 == 1? r1 : r2;
		rule = 150;
		state = do_rule_forward(rule, ivec2(0));
	}
	// if (frames < 256)
	// 	state = init_rule(rule);
	// else if (frames == 256)
	// 	state = swap();
	// else if (frames < 256)

	// if (frames < 256)
	// 	state = init_rule(r3);
	// else if (frames == 256)
	// 	state = swap();
	// else if (frames < 256 + 256*2)
	// 	state = init_rule(frames % 2 == 1 ? r1 : r2);
	// else {
		// if (frames % 6 < 3) {
		// 	if (frames % 3 == 0)
		// 		rule = r2;
		// 	if (frames % 3 == 1)
		// 		rule = r1;
		// 	if (frames % 3 == 2)
		// 		rule = r3;

		// 	state = do_inverse(rule);
		// }
		// else {
		// 	state = do_rule(rule);
		// }

		// if (frames % 3 == 2)
		// 	state = do_inverse(rule);
		// else
		// 	state = do_rule(rule);
		// if (frames % 6 == 0)
		// 	state = do_rule(r1);
		// if (frames % 6 == 1)
		// 	state = do_rule(r2);
		// if (frames % 6 == 2)
		// 	state = do_rule(r3);
		// if (frames % 6 == 3)
		// 	state = do_inverse(r2);
		// if (frames % 6 == 4)
		// 	state = do_inverse(r1);
		// if (frames % 6 == 5)
		// 	state = do_inverse(r3);
	// }
	// else
		// state = do_rule(r3);
		// state = do_inverse(r3);
	float m = state.x;
	float prev = state.y;
	// if (stage == 3) {
	// 	m = state.y;
	// 	prev = state.x;
	// }


//	----    ----    ----    ----    ----    ----    ----    ----
//	Shader Output
//	----    ----    ----    ----    ----    ----    ----    ----
	if(frames <= 0u || cmd == 1u) {
		if (pos == origin) {
			m = 1;
			prev = 1;
		}
		else  {
			m = 0;
			prev = 0;
		}
		if (random && pos.y == origin.y) {
			m = rand(pos) > 0.5 ? 1 : 0;
		}
	}

	if( cmd == 2u ) {
		m = 0.0;
	}
	
	// out_col1 = vec4(vec3(m, 0.99*m + 0.01*prev, m), 1);
	out_col1 = vec4(vec3(m, prev, m), 1);
}
