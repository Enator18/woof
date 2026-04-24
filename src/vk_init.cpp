#include "volk.h"
#include "VkBootstrap.h"
#include "SDL3/SDL_vulkan.h"
#include "vk_mem_alloc.h"
#include "vk_init.h"

extern "C"
{
    #include "i_video.h"
}

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

VkInstance instance;
VkSurfaceKHR surface;
VkPhysicalDevice physDevice;
VkDevice device;
VkQueue queue;
uint32_t queueFamily;
VmaAllocator allocator;
VkSwapchainKHR swapchain;
uint32_t imageCount;
VkImage* swapImages;
VkImageView* swapViews;

void VK_Init()
{
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    vkb::InstanceBuilder builder{vkGetInstanceProcAddr};
    vkb::Instance vkbInstance = builder
                      .set_app_name("Woof")
                      .request_validation_layers()
                      .use_default_debug_messenger()
                      .require_api_version(1, 3)
                      .build().value();
    instance = vkbInstance.instance;

    volkLoadInstanceOnly(instance);

    SDL_Vulkan_CreateSurface((SDL_Window*)I_GetSDLWindow(), instance, nullptr, &surface);

    VkPhysicalDeviceVulkan12Features feat12{};
    feat12.bufferDeviceAddress = true;
    feat12.descriptorIndexing = true;
    feat12.shaderSampledImageArrayNonUniformIndexing = true;
    feat12.runtimeDescriptorArray = true;

    VkPhysicalDeviceVulkan13Features feat13{};
    feat13.synchronization2 = true;

    vkb::PhysicalDeviceSelector selector{vkbInstance};
    vkb::PhysicalDevice vkbPhysDevice = selector
                                            .set_surface(surface)
                                            .set_minimum_version(1, 3)
                                            .set_required_features_12(feat12)
                                            .set_required_features_13(feat13)
                                            .prefer_gpu_device_type()
                                            .select().value();
    physDevice = vkbPhysDevice.physical_device;

    vkb::DeviceBuilder deviceBuilder{vkbPhysDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();
    device = vkbDevice.device;
    queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    queueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    volkLoadDevice(device);

    VmaAllocatorCreateInfo allocInfo = {};
    allocInfo.physicalDevice = physDevice;
    allocInfo.device = device;
    allocInfo.instance = instance;
    allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VmaVulkanFunctions vmaFuncs;
    VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocInfo, &vmaFuncs));

    allocInfo.pVulkanFunctions = &vmaFuncs;

    VK_CHECK(vmaCreateAllocator(&allocInfo, &allocator));
}

void VK_InitFramebuffers(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapBuilder{physDevice, device, surface};

    vkb::Swapchain vkbSwapchain = swapBuilder
                                      .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_desired_extent(width, height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .build().value();

    swapchain = vkbSwapchain.swapchain;
    imageCount = vkbSwapchain.image_count;
    swapImages = (VkImage*)malloc(imageCount);
    swapViews = (VkImageView*)malloc(imageCount);
    memcpy(swapImages, vkbSwapchain.get_images().value().data(), sizeof(VkImage) * imageCount);
    memcpy(swapViews, vkbSwapchain.get_image_views().value().data(), sizeof(VkImageView) * imageCount);
}