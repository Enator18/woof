#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <volk.h>

#include "vk_buffer.h"
#include "vk_main.h"

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

vk_buffer_t VK_CreateBuffer(size_t size, VkBufferUsageFlags usage)
{
    vk_buffer_t result;
    VkBufferCreateInfo bufferInfo =
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = size,
        .usage = usage
    };

    VmaAllocationCreateFlags allocFlags;
    VkMemoryPropertyFlagBits requiredFlags;

    if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
    {
        allocFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    else
    {
        allocFlags = 0;
        requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    VmaAllocationCreateInfo allocInfo =
    {
        .flags = allocFlags,
        .requiredFlags = requiredFlags,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &result.buffer, &result.allocation, &result.info));

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = result.buffer
        };
        result.address = vkGetBufferDeviceAddress(device, &addressInfo);
    }
    else
    {
        result.address = 0;
    }

    return result;
}

void VK_DestroyBuffer(vk_buffer_t buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

//Create a temporary command buffer for executing commands outside of the rendering loop
VkCommandBuffer BeginImmediateCommands()
{
    VkCommandBufferAllocateInfo allocInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = cmdPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
};

//Delete a command buffer and submit the commands in it
void EndImmediateCommands(VkCommandBuffer commandBuffer)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkCommandBufferSubmitInfo commandInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer
    };

    VkSubmitInfo2 submitInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo
    };

    VK_CHECK(vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
};

void VK_CopyToBuffer(vk_buffer_t buffer, void* src, size_t size)
{
    vk_buffer_t stagingBuffer = VK_CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    void* stagingData = stagingBuffer.info.pMappedData;
    memcpy(stagingData, src, size);

    VkCommandBuffer cmd = BeginImmediateCommands();

    VkBufferCopy copy =
    {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };

    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer.buffer, 1, &copy);

    EndImmediateCommands(cmd);
    VK_DestroyBuffer(stagingBuffer);
}