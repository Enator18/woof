#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require

struct DrawColumn
{
    uint pos;
    uint props;
};
layout (buffer_reference, std430) buffer ColBuffer
{
    uint count;
    uint padding;
    DrawColumn vals[];
};

#define SUBSECTOR_FLAG 0x80000000u
#define SIDE_FLAG 0x80000000u

struct Node
{
    uvec2 lineStart;
    uvec2 lineExtent;
    uvec4 boxA;
    uvec4 boxB;
    uint childA;
    uint childB;
};
layout (buffer_reference, std430) readonly buffer NodeBuffer
{
    Node vals[];
};

struct Subsector
{
    uint sector;
    uint numlines;
    uint firstline;

    uint padding;
};
layout (buffer_reference, std430) readonly buffer SubsectorBuffer
{
    Subsector vals[];
};

struct Sector
{
    uint floorheight;
    uint ceilingheight;
};
layout (buffer_reference, std430) readonly buffer SectorBuffer
{
    Sector vals[];
};

struct Seg
{
    uint x1;
    uint y1;
    uint x2;
    uint y2;
    uint length;
    uint angle;
    uint side;
    int back;
};
layout (buffer_reference, std430) readonly buffer SegBuffer
{
    Seg vals[];
};

layout (push_constant, std430) uniform PushConstants
{
    ColBuffer colBuffer;
    NodeBuffer nodeBuffer;
    SubsectorBuffer subsectorBuffer;
    SectorBuffer sectorBuffer;
    SegBuffer segBuffer;
    uvec3 viewPos;
    vec2 firstRay;
    vec2 rayInc;
    uint viewHeight;
    uint rootNode;
} pcs;

bool PointOnSide(Node node)
{
    if(node.lineExtent.x == 0)
    {
        return pcs.viewPos.x <= node.lineStart.x ? node.lineExtent.y > 0 : node.lineExtent.y < 0;
    }

    if(node.lineExtent.y == 0)
    {
        return pcs.viewPos.y <= node.lineStart.y ? node.lineExtent.x < 0 : node.lineExtent.x > 0;
    }

    uint relX = pcs.viewPos.x - node.lineStart.x;
    uint relY = pcs.viewPos.y - node.lineStart.y;

    // Try to quickly decide by looking at sign bits.
    if((node.lineExtent.y ^ node.lineExtent.x ^ relX ^ relY) < 0)
    {
        return (node.lineExtent.y ^ relX) < 0;  // (left is negative)
    }

    return (int64_t(relY) * int64_t(node.lineExtent.x)) >= (int64_t(relX) * int64_t(node.lineExtent.y));
}

layout (constant_id = 0) const uint STACK_SIZE = 4;

layout(local_size_x_id = 1) in;
void main()
{
    uint stackPos = 0;
    uint nodeStack[STACK_SIZE];
    uint current = pcs.rootNode;
    uint floorClip = pcs.viewHeight;
    uint ceilingClip = -1;
    bool open = true;
    while (true)
    {
        if ((current & SUBSECTOR_FLAG) == 0)
        {
            Node node = pcs.nodeBuffer.vals[current];
            uint back;
            uvec4 backBox;
            if (PointOnSide(node))
            {
                current = node.childB;
                back = node.childA;
                backBox = node.boxA;
            }
            else
            {
                current = node.childA;
                back = node.childB;
                backBox = node.boxB;
            }

            vec2 ray = pcs.firstRay + pcs.rayInc * gl_GlobalInvocationID.x;
            vec2 tMin = (backBox.zy - pcs.viewPos.xy) / ray;
            vec2 tMax = (backBox.wx - pcs.viewPos.xy) / ray;
            vec2 t1 = min(tMin, tMax);
            vec2 t2 = max(tMin, tMax);
            float tNear = max(t1.x, t1.y);
            float tFar = min(t2.x, t2.y);

            if (subgroupAny(open && tFar > 0 && tNear <= tFar))
            {
                if (gl_SubgroupInvocationID == stackPos % gl_SubgroupSize)
                {
                    nodeStack[stackPos / gl_SubgroupSize] = back;
                }
                stackPos++;
            }
        }
        else
        {
            Subsector subsector = pcs.subsectorBuffer.vals[current & ~SUBSECTOR_FLAG];
            Sector sector = pcs.sectorBuffer.vals[subsector.sector];

            // todo Draw subsector
            for (uint i = 0; i < subsector.numlines; i++)
            {
                Seg seg = pcs.segBuffer.vals[subsector.firstline + i];
                if (open)
                {

                }
            }

            if (stackPos == 0)
            {
                break;
            }
            stackPos--;

            current = subgroupBroadcast(nodeStack[stackPos / gl_SubgroupSize], stackPos % gl_SubgroupSize);
        }
    }
}
