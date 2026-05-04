#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "doomtype.h"

typedef struct vk_buffer_s
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkDeviceAddress address;
} vk_buffer_t;

vk_buffer_t VK_CreateBuffer(size_t size, VkBufferUsageFlags usage, boolean mapped);
void VK_DestroyBuffer(vk_buffer_t buffer);
void VK_CopyToBuffer(vk_buffer_t buffer, void* src, size_t size);
#endif //VK_BUFFER_H
