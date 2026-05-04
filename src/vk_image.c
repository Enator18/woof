#include <stdio.h>
#include <stdlib.h>
#include <volk.h>

#include "vk_image.h"
#include "vk_main.h"

vk_image_t VK_CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageCreateFlags flags, VkImageUsageFlags usage)
{
    vk_image_t result;
    VkImageCreateInfo info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .flags = flags,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage
    };

    VmaAllocationCreateInfo allocInfo =
    {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VK_CHECK(vmaCreateImage(allocator, &info, &allocInfo, &result.image, &result.allocation, NULL));

    result.view = VK_CreateImageView(result.image, format);
    return result;
}

VkImageView VK_CreateImageView(VkImage image, VkFormat format)
{
    VkImageViewCreateInfo texViewInfo =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange =
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkImageView result;
    VK_CHECK(vkCreateImageView(device, &texViewInfo, NULL, &result));
    return result;
}

void VK_DestroyImage(vk_image_t image)
{
    vkDestroyImageView(device, image.view, NULL);
    vmaDestroyImage(allocator, image.image, image.allocation);
}

VkImageMemoryBarrier2 VK_ImageBarrier(VkImage image, VkImageLayout currentLayout,
    VkImageLayout newLayout, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage)
{
    VkImageMemoryBarrier2 imageBarrier =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStage,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = dstStage,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = currentLayout,
        .newLayout = newLayout,
    };

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || currentLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
            ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageSubresourceRange subImage =
    {
        .aspectMask = aspectMask,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS
    };

    imageBarrier.subresourceRange = subImage;
    imageBarrier.image = image;

    return imageBarrier;
}
