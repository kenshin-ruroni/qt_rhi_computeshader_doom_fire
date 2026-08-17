#version 460

// Input vertex attributes from your vertex buffer
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Output to the fragment shader
layout(location = 0) out vec2 vTexCoord;

void main() {
    // Pass the texture coordinates to the fragment shader
    vTexCoord = inTexCoord;

    // Output standard clip-space position (Z=0.0, W=1.0)
    gl_Position = vec4(inPosition, 0.0, 1.0);
}

