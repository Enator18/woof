#ifndef VK_MAIN_H
#define VK_MAIN_H

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include "doomtype.h"
#include "vk_buffer.h"

#define VK_CHECK(x)                                                   \
    do                                                                \
    {                                                                 \
        VkResult err = (x);                                           \
        if (err)                                                      \
        {                                                             \
            printf("Detected Vulkan error: %i at line %i in file %s", \
                err, __LINE__, __FILE__);                             \
            abort();                                                  \
        }                                                             \
    } while (0)

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
    vk_buffer_t columnBuffer;
} vk_framedata_t;

typedef struct vk_pushconst_s
{
    VkDeviceAddress buffer;
    uint32_t count;
} vk_pushconst_t;

extern VkInstance instance;
extern VkSurfaceKHR surface;
extern VkPhysicalDevice physDevice;
extern VkDevice device;
extern VkQueue queue;
extern uint32_t queueFamily;
extern VmaAllocator allocator;
extern VkSwapchainKHR swapchain;
extern boolean vkVsync;
extern uint32_t swapWidth;
extern uint32_t swapHeight;
extern uint32_t swapImageCount;
extern vk_swapimage_t* swapImages;
extern VkCommandPool cmdPool;
extern vk_image_t frameIndexed;
extern vk_image_t frameColor;
extern VkImageView frameColorIntView;
extern vk_buffer_t paletteBuffer;
extern vk_framedata_t frames[FRAMES_IN_FLIGHT];
extern VkDescriptorPool descPool;
extern VkDescriptorSetLayout transferDescLayout;
extern VkDescriptorSet transferDescSet;
extern VkPipelineLayout transferLayout;
extern VkPipeline transferPipeline;
extern VkPipeline drawPipeline;

void VK_CreateFramebuffers(uint32_t width, uint32_t height);
void VK_DestroyFramebuffers(void);
void VK_RecreateSwapchain(void);
void VK_AddColumn(uint16_t x, uint16_t y, uint16_t height);
void VK_DrawFrame(void);

#endif //VK_MAIN_H
