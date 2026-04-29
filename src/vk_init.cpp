#include "volk.h"
#include "VkBootstrap.h"
#include "SDL3/SDL_vulkan.h"
#include "vk_mem_alloc.h"
#include "vk_init.h"

extern "C"
{
    #include "i_printf.h"
    #include "i_video.h"
    #include "vk_buffer.h"
    #include "vk_image.h"
    #include "vk_main.h"
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

VkBool32 DebugMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    verbosity_t verbosity = VB_INFO;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        verbosity = VB_WARNING;
    }
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        verbosity = VB_ERROR;
    }

    I_Printf(verbosity, "%s", pCallbackData->pMessage);

    return VK_FALSE;
}

void VK_Init(byte* paletteData)
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

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = queueFamily;
    cmdPoolInfo.pNext = nullptr;

    VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));

    VmaVulkanFunctions vmaFuncs;
    VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocInfo, &vmaFuncs));

    allocInfo.pVulkanFunctions = &vmaFuncs;

    VK_CHECK(vmaCreateAllocator(&allocInfo, &allocator));

    paletteBuffer = VK_CreateBuffer(4 * 256 * 14,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    byte colors[4 * 256 * 14];

    for (uint32_t i = 0; i < 256 * 14; i++)
    {
        colors[i * 4] = paletteData[i * 3];
        colors[i * 4 + 1] = paletteData[i * 3 + 1];
        colors[i * 4 + 2] = paletteData[i * 3 + 2];
        colors[i * 4 + 3] = 255;
    }

    VK_CopyToBuffer(paletteBuffer, colors, 4 * 256 * 14);

    VkCommandBufferAllocateInfo cmdInfo = {};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.commandBufferCount = 1;
    cmdInfo.commandPool = cmdPool;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.pNext = nullptr;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.flags = 0;
    semaphoreInfo.pNext = nullptr;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    fenceInfo.pNext = nullptr;

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdInfo, &frameData[i].cmd));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
            &frameData[i].acquireSemaphore));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frameData[i].fence));
    }
}

void VK_InitSwapchain(uint32_t width, uint32_t height, boolean vsync)
{
    vkb::SwapchainBuilder swapBuilder{physDevice, device, surface};

    VkPresentModeKHR presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    vkb::Swapchain vkbSwapchain = swapBuilder
        .set_desired_format({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(presentMode)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build().value();

    const std::vector<VkImage>& swapImageHandles = vkbSwapchain.get_images().value();
    const std::vector<VkImageView>& swapImageViews = vkbSwapchain.get_image_views().value();

    swapchain = vkbSwapchain.swapchain;
    imageCount = vkbSwapchain.image_count;
    swapImages = (vk_swapimage_t*)malloc(imageCount * sizeof(vk_swapimage_t));

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.flags = 0;
    semaphoreInfo.pNext = nullptr;

    for (uint32_t i = 0; i < imageCount; i++)
    {
        swapImages[i].image = swapImageHandles[i];
        swapImages[i].view = swapImageViews[i];
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
            &swapImages[i].renderSemaphore));
    }
}

void VK_DestroySwapchain()
{
    for (uint32_t i = 0; i < imageCount; i++)
    {
        vkDestroySemaphore(device, swapImages[i].renderSemaphore, nullptr);
        vkDestroyImageView(device, swapImages[i].view, nullptr);
        vkDestroyImage(device, swapImages[i].image, nullptr);
    }
    free(swapImages);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void VK_CreateFramebuffers(uint32_t width, uint32_t height)
{
    frameIndexed = VK_CreateImage(width, height, VK_FORMAT_R8_UINT, 0,
        VK_IMAGE_USAGE_STORAGE_BIT);
    frameColor = VK_CreateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    frameColorIntView = VK_CreateImageView(frameColor.image, VK_FORMAT_R32_UINT);
}

void VK_DestroyFramebuffers()
{
    if (device != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, frameColorIntView, nullptr);
        VK_DestroyImage(frameColor);
        VK_DestroyImage(frameIndexed);
        frameColor = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        frameIndexed = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    }
}