#ifndef VK_MAIN_H
#define VK_MAIN_H

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include "doomtype.h"
#include "m_fixed.h"
#include "tables.h"
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
} vk_framedata_t;

typedef struct vk_subsector_s
{
    uint32_t sector;
    uint32_t numlines;
    uint32_t firstline;

    uint32_t padding;
} vk_subsector_t;

typedef struct vk_sector_s
{
    fixed_t floorheight;
    fixed_t ceilingheight;
} vk_sector_t;

typedef struct vk_seg_s
{
    fixed_t x1, y1;
    fixed_t x2, y2;
    uint32_t length;
    angle_t angle;
    uint32_t side;
    int32_t back;
} vk_seg_t;

typedef struct vk_drawcol_s
{
    uint32_t pos;
    uint32_t props;
} vk_drawcol_t;

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
extern VkPipeline bspPipeline;

void VK_CreateFramebuffers(uint32_t width, uint32_t height);
void VK_DestroyFramebuffers(void);
void VK_LoadMap(void);
void VK_RecreateSwapchain(void);
void VK_DrawFrame(void);

#endif //VK_MAIN_H
