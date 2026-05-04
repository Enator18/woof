#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

typedef struct vk_image_s
{
    VkImage image;
    VmaAllocation allocation;
    VkImageView view;
} vk_image_t;

vk_image_t VK_CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageCreateFlags flags, VkImageUsageFlags usage);
VkImageView VK_CreateImageView(VkImage image, VkFormat format);
void VK_DestroyImage(vk_image_t image);
VkImageMemoryBarrier2 VK_ImageBarrier(VkImage image, VkImageLayout currentLayout,
    VkImageLayout newLayout, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage);

#endif //VK_IMAGE_H
