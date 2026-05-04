#version 460

#extension GL_EXT_buffer_reference : require

struct DrawColumn
{
    uint pos;
    uint props;
};

layout (buffer_reference, std430) readonly buffer ColBuffer
{
    DrawColumn cols[];
};

layout (push_constant, std430) uniform PushConstants
{
    ColBuffer columnBuffer;
    uint columnCount;
} pcs;

layout(set = 0, binding = 0, r8ui) uniform uimage2D frameIndexed;

layout (local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint index = gl_GlobalInvocationID.x;

    if (index < pcs.columnCount)
    {
        DrawColumn column = pcs.columnBuffer.cols[index];
        uint x = column.pos & 0x0000FFFFu;
        uint y = column.pos >> 16;
        uint height = column.props & 0x0000FFFFu;
        uint color = column.props >> 16;
        for (uint i = 0; i < height; i++)
        {
            imageStore(frameIndexed, ivec2(x, y + i), uvec4(color, 0, 0, 0));
        }
    }
}
