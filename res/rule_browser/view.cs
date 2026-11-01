#version 460

layout(local_size_x = 16, local_size_y = 16) in;
uniform sampler2DArray source_texture;
layout(rgba32f, binding = 1) writeonly uniform image2DArray display_image;
uniform bool browser_enabled;
uniform int columns = 4;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 output_size = imageSize(display_image).xy;
    if (any(greaterThanEqual(pixel, output_size)))
        return;
    ivec2 input_size = textureSize(source_texture, 0).xy;
    vec2 uv = (vec2(pixel) + 0.5) / vec2(output_size);
    int layer = 0;
    if (browser_enabled) {
        ivec2 cell = min(ivec2(uv * float(columns)), ivec2(columns - 1));
        layer = (columns - 1 - cell.y) * columns + cell.x;
        uv = fract(uv * float(columns));
    }
    ivec2 source = min(ivec2(uv * vec2(input_size)), input_size - 1);
    imageStore(display_image, ivec3(pixel, 0),
               vec4(texelFetch(source_texture, ivec3(source, layer), 0).rgb, 1.0));
}
