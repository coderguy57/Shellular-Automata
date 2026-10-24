#version 460

layout(location = 0) out vec4 out_col;
in vec2 v_texcoord;
uniform sampler2DArray tex;

void main() {
    out_col = vec4(texture(tex, vec3(v_texcoord, 0)).rgb, 1.0);
}
