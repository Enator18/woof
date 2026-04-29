#include <volk.h>
#include <vk_mem_alloc.h>

#include "vk_buffer.h"
#include "vk_image.h"
#include "vk_main.h"

VkInstance instance;
VkSurfaceKHR surface;
VkPhysicalDevice physDevice;
VkDevice device;
VkQueue queue;
uint32_t queueFamily;
VmaAllocator allocator;
VkSwapchainKHR swapchain;
uint32_t imageCount;
vk_swapimage_t* swapImages;
VkCommandPool cmdPool;
vk_image_t frameIndexed;
vk_image_t frameColor;
VkImageView frameColorIntView;
vk_buffer_t paletteBuffer;
vk_framedata_t frameData[FRAMES_IN_FLIGHT];
