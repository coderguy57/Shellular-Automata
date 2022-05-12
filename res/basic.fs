#version 460
 
out vec4 frag_colour;
 
in vec2 v_texcoord;

uniform sampler2DArray tex;

void main() {
    frag_colour = texture(tex, vec3(v_texcoord, 0));

    //! Mixed layers
    // vec4 layer1 = texture(tex, vec3(v_texcoord, 0));
    // layer1.r = mix(layer1.r, layer1.a, 0.5);
    // vec4 layer2 = texture(tex, vec3(v_texcoord, 1));
    // layer2.r = mix(layer2.r, layer2.a, 0.5);
    // frag_colour = mix(layer1, layer2, 0.5);

    //! Split layers
    // if (v_texcoord.x < 1./3.) {
    //     vec2 coord = v_texcoord;
    //     coord.x = coord.x * 3.;
    //     frag_colour = texture(tex, vec3(coord, 0), 0);
    //     frag_colour.a = 1;
    // } else if (v_texcoord.x < 2./3.) {
    //     vec2 coord = v_texcoord;
    //     coord.x = (coord.x - 1./3.) * 3.;
    //     frag_colour = texture(tex, vec3(coord, 1), 2);
    //     frag_colour.a = 1;
    // } else {
    //     vec2 coord = v_texcoord;
    //     coord.x = (coord.x - 2./3.) * 3.;
    //     float a1 = texture(tex, vec3(coord, 0), 0).a;
    //     float a2 = texture(tex, vec3(coord, 1), 0).a;
    //     frag_colour = vec4(a1, a2, 0, 1.);
    // }
    // frag_colour.r = 0;
    // frag_colour.g = 0;
    // frag_colour.b = 2. * abs(frag_colour.b - 0.5);
    // frag_colour.b = mix(frag_colour.b, frag_colour.a, 0.5);

    // Post processing
    frag_colour.rgb *= 0.7;
    frag_colour.rgb = pow(frag_colour.rgb, vec3(1. / 2.2)); // SRGB -> RGB
    frag_colour.a = 1;
}