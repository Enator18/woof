#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

struct DrawColumn
{
    u16vec2 pos;
    uint16_t height;
    uint16_t color;
};

layout (buffer_reference, std430) readonly buffer ColBuffer
{
    uint count;
    DrawColumn vals[];
};

layout (push_constant, std430) uniform PushConstants
{
    ColBuffer colBuffer;
} pcs;

layout(set = 0, binding = 0, r8ui) uniform uimage2D frameIndexed;

layout (local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint index = gl_GlobalInvocationID.x;

    if (index < pcs.colBuffer.count)
    {
        DrawColumn column = pcs.colBuffer.vals[index];
        for (uint i = 0; i < column.height; i++)
        {
            imageStore(frameIndexed, ivec2(column.pos.x, column.pos.y + i), uvec4(column.color, 0, 0, 0));
        }
    }
}
