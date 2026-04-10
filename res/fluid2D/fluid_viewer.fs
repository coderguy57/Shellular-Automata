#version 460
 
out vec4 frag_colour;
 
in vec2 v_texcoord;

//! option "Smoothing" (0, 1)
uniform float smoothing = 0.5;

uniform sampler2DArray tex;
uniform sampler2DArray prev_tex;

void main() {
    vec4 prev_color = texture(prev_tex, vec3(v_texcoord, 0));

    float col1 = texture(tex, vec3(v_texcoord, 0)).z;
    float col2 = texture(tex, vec3(v_texcoord, 1)).z;
    vec4 new_col = vec4(col1, col2, 0, 1);

    frag_colour = mix(prev_color, new_col, smoothing);
}