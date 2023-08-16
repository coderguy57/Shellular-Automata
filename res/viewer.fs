#version 460
 
out vec4 frag_colour;
 
in vec2 v_texcoord;

//! option "Smoothing" (0, 1)
uniform float smoothing = 0.5;

uniform sampler2DArray tex;
uniform sampler2DArray prev_tex;

void main() {
    vec4 prev_color = texture(prev_tex, vec3(v_texcoord, 0));
    frag_colour = texture(tex, vec3(v_texcoord, 0));

    frag_colour = mix(prev_color, frag_colour, smoothing);
    // frag_colour = mix(prev_color, frag_colour, smoothing);

    // Post processing
    // frag_colour.rgb *= 0.7;
    // frag_colour.a = 1;
}