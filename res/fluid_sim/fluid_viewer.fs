#version 460
 
out vec4 frag_colour;
 
in vec2 v_texcoord;

uniform sampler2DArray tex;

void main() {
    frag_colour = texture(tex, vec3(v_texcoord, 0));

    frag_colour.rg = frag_colour.zw;
    frag_colour.b = 0;
    frag_colour.rgb *= 0.7;
    frag_colour.rgb = pow(frag_colour.rgb, vec3(1. / 2.2)); // SRGB -> RGB
    frag_colour.a = 1;
}