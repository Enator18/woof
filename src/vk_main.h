#ifndef VK_MAIN_H
#define VK_MAIN_H

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

typedef struct vk_buffer_s vk_buffer_t;
typedef struct vk_image_s vk_image_t;

#define FRAMES_IN_FLIGHT 2

typedef struct vk_swapimage_s
{
    VkImage image;
    VkImageView view;
    VkSemaphore renderSemaphore;
} vk_swapimage_t;

typedef struct vk_framedata_s
{
    VkCommandBuffer cmd;
    VkSemaphore acquireSemaphore;
    VkFence fence;
} vk_framedata_t;

extern VkInstance instance;
extern VkSurfaceKHR surface;
extern VkPhysicalDevice physDevice;
extern VkDevice device;
extern VkQueue queue;
extern uint32_t queueFamily;
extern VmaAllocator allocator;
extern VkSwapchainKHR swapchain;
extern uint32_t imageCount;
extern vk_swapimage_t* swapImages;
extern VkCommandPool cmdPool;
extern vk_image_t frameIndexed;
extern vk_image_t frameColor;
extern VkImageView frameColorIntView;
extern vk_buffer_t paletteBuffer;
extern vk_framedata_t frameData[FRAMES_IN_FLIGHT];

void VK_SetPalette(void* colors);

#endif //VK_MAIN_H
