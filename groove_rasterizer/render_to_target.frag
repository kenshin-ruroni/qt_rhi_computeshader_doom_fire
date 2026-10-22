#version 460

// Input from the vertex shader
layout(location = 0) in vec2 vTexCoord;

// Output color to the frame buffer
layout(location = 0) out vec4 fragColor;

// Texture and sampler binding (set 0, binding 1)
layout(binding = 2) uniform sampler2D texSampler;

void main() {
    // Sample the color from the texture using the coordinates
    fragColor = texture(texSampler, vTexCoord);
}
