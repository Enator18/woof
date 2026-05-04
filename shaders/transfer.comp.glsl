#version 460

#extension GL_EXT_buffer_reference : require

layout (buffer_reference, std430) readonly buffer PalBuffer
{
    uint colors[];
};

layout (push_constant, std430) uniform PushConstants
{
    PalBuffer palette;
} pcs;

layout(set = 0, binding = 0, r8ui) uniform uimage2D frameIndexed;
layout(set = 0, binding = 1, r32ui) uniform uimage2D frameColor;

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    ivec2 texCoord = ivec2(gl_GlobalInvocationID);
    ivec2 frameSize = imageSize(frameIndexed);
    if (texCoord.x < frameSize.x && texCoord.y < frameSize.y)
    {
        uvec4 indexedVal = imageLoad(frameIndexed, texCoord);
        uint color = pcs.palette.colors[indexedVal.x];
        imageStore(frameColor, texCoord, uvec4(color, 0, 0, 0));
    }
}