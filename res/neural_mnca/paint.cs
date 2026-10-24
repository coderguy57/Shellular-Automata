#version 460

layout(rgba32f, binding = 0) uniform image2DArray inputImage;
layout(rgba32f, binding = 1) uniform image2DArray outputImage;

uniform vec3 paint_color;
uniform vec3 paint_mask;
uniform int paint_size;
uniform int paint_layer;
uniform bool paint_smooth;
uniform vec2 cursor_pos;
uniform vec2 last_cursor_pos;
uniform ivec2 mlr;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID);
    ivec3 image_size = imageSize(outputImage);
    if (any(greaterThanEqual(coord, image_size))) {
        return;
    }

    vec4 color = imageLoad(inputImage, coord);
    if (coord.z == paint_layer) {
        vec2 start = last_cursor_pos * vec2(image_size.xy);
        vec2 end = cursor_pos * vec2(image_size.xy);
        vec2 segment = end - start;
        vec2 pixel = vec2(coord.xy) + 0.5;
        float length_squared = dot(segment, segment);
        float t = length_squared > 0.0 ? clamp(dot(pixel - start, segment) / length_squared, 0.0, 1.0) : 0.0;
        float distance_to_stroke = length(pixel - (start + t * segment));
        float radius = float(max(paint_size, 1));
        float amount = paint_smooth ? max(1.0 - distance_to_stroke / radius, 0.0) : 1.0 - step(radius, distance_to_stroke);
        vec3 target = mlr.y != 0 ? vec3(0.0) : paint_color;
        color.rgb = mix(color.rgb, target, amount * paint_mask);
        color.a = 1.0;
    }
    imageStore(outputImage, coord, color);
}

