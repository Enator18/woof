#include <stdio.h>
#include <stdlib.h>
#include <volk.h>
#include <vk_mem_alloc.h>

#include "doomtype.h"
#include "r_state.h"
#include "vk_buffer.h"
#include "vk_image.h"
#include "vk_init.h"
#include "vk_main.h"

VkInstance instance;
VkSurfaceKHR surface;
VkPhysicalDevice physDevice;
VkDevice device;
VkQueue queue;
uint32_t queueFamily;
VmaAllocator allocator;
VkSwapchainKHR swapchain;
boolean vkVsync;
uint32_t swapWidth;
uint32_t swapHeight;
uint32_t swapImageCount;
vk_swapimage_t* swapImages;
VkCommandPool cmdPool;
uint32_t frameWidth;
uint32_t frameHeight;
vk_image_t frameIndexed;
vk_image_t frameColor;
VkImageView frameColorIntView;
vk_buffer_t paletteBuffer;
vk_buffer_t columnBuffer;
vk_buffer_t nodeBuffer;
vk_buffer_t subsectorBuffer;
vk_buffer_t sectorBuffer;
vk_buffer_t segBuffer;

vk_drawcol_t* columnUpload;
uint32_t columnCount;
vk_framedata_t frames[FRAMES_IN_FLIGHT];
VkDescriptorPool descPool;
VkDescriptorSetLayout transferDescLayout;
VkDescriptorSet transferDescSet;
VkPipelineLayout transferLayout;
VkPipeline transferPipeline;
VkPipeline drawPipeline;
VkPipeline bspPipeline;
uint32_t frameNum;

void VK_CreateFramebuffers(uint32_t width, uint32_t height)
{
    frameWidth = width;
    frameHeight = height;
    frameIndexed = VK_CreateImage(width, height, VK_FORMAT_R8_UINT, 0,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    frameColor = VK_CreateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    frameColorIntView = VK_CreateImageView(frameColor.image, VK_FORMAT_R32_UINT);

    VkDescriptorImageInfo imageInfos[2] =
    {
        {
            .imageView = frameIndexed.view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        },
        {
            .imageView = frameColorIntView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        }
    };

    VkWriteDescriptorSet descWrite =
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = transferDescSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = imageInfos
    };

    vkUpdateDescriptorSets(device, 1, &descWrite, 0, NULL);

    size_t colBufferSize = frameWidth * frameHeight * 8;
    columnBuffer = VK_CreateBuffer(colBufferSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, false);

    columnUpload = malloc(colBufferSize);
}

void VK_DestroyFramebuffers(void)
{
    vkQueueWaitIdle(queue);
    if (device != VK_NULL_HANDLE && frameColor.image != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, frameColorIntView, NULL);
        VK_DestroyImage(frameColor);
        VK_DestroyImage(frameIndexed);
        frameColor.image = VK_NULL_HANDLE;
        VK_DestroyBuffer(columnBuffer);
        free(columnUpload);
    }
}

int GetMaxDepth(int node)
{
    if (node & NF_SUBSECTOR)
    {
        return 0;
    }
    int depthA = GetMaxDepth(nodes[node].children[0]);
    int depthB = GetMaxDepth(nodes[node].children[1]);
    return 1 + max(depthA, depthB);
}

void VK_LoadMap(void)
{
    // Nodes
    nodeBuffer = VK_CreateBuffer(numnodes * sizeof(node_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, false);
    VK_CopyToBuffer(nodeBuffer, nodes, numnodes * sizeof(node_t));

    // Subsectors and segs
    vk_subsector_t* vkSubsectors = malloc(numsubsectors * sizeof(vk_subsector_t));
    vk_seg_t* vkSegs = malloc(numsegs * sizeof(vk_seg_t));
    uint32_t realNumSegs = 0;
    for (uint32_t i = 0; i < numsubsectors; i++)
    {
        subsector_t* subsector = subsectors + i;
        vk_subsector_t vkSubsector =
        {
            .sector = subsector->sector - sectors,
            .firstline = realNumSegs
        };
        uint32_t numlines = 0;
        for (uint32_t line = 0; line < subsector->numlines; line++)
        {
            seg_t* seg = segs + subsector->firstline + line;
            if (seg->linedef)
            {
                vk_seg_t vkSeg =
                {
                    .x1 = seg->v1->r_x,
                    .y1 = seg->v1->r_y,
                    .x2 = seg->v2->r_x,
                    .y2 = seg->v2->r_y,
                    .length = seg->r_length,
                    .angle = seg->r_angle,
                    .side = seg->sidedef - sides,
                    .back = seg->backsector ? (seg->backsector - sectors) : -1
                };
                vkSegs[realNumSegs] = vkSeg;
                realNumSegs++;
                numlines++;
            }
        }
        vkSubsector.numlines = numlines;
        vkSubsectors[i] = vkSubsector;
    }
    subsectorBuffer = VK_CreateBuffer(numsubsectors * sizeof(vk_subsector_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, false);
    VK_CopyToBuffer(subsectorBuffer, vkSubsectors, numsubsectors * sizeof(vk_subsector_t));
    free(vkSubsectors);

    segBuffer = VK_CreateBuffer(realNumSegs * sizeof(vk_seg_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, false);
    VK_CopyToBuffer(segBuffer, vkSegs, realNumSegs * sizeof(vk_seg_t));
    free(vkSegs);

    // Sectors
    vk_sector_t* vkSectors = malloc(numsectors * sizeof(vk_sector_t));
    for (uint32_t i = 0; i < numsectors; i++)
    {
        vk_sector_t vkSector =
        {
            .floorheight = sectors[i].floorheight,
            .ceilingheight = sectors[i].ceilingheight
        };
        vkSectors[i] = vkSector;
    }
    sectorBuffer = VK_CreateBuffer(numsectors * sizeof(vk_sector_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, false);
    VK_CopyToBuffer(sectorBuffer, vkSectors, numsectors * sizeof(vk_sector_t));
    free(vkSectors);
}

void VK_RecreateSwapchain(void)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &surfaceCapabilities);

    uint32_t width = surfaceCapabilities.currentExtent.width;
    uint32_t height = surfaceCapabilities.currentExtent.height;

    if (width == 0 || height == 0)
    {
        return;
    }

    if (width == swapWidth && height == swapHeight)
    {
        return;
    }

    vkDeviceWaitIdle(device);

    VK_DestroySwapchain();

    VK_InitSwapchain(width, height, vkVsync);
}

void VK_DrawFrame(void)
{
    VK_RecreateSwapchain();
    VkCommandBuffer cmd = frames[frameNum].cmd;
    VK_CHECK(vkWaitForFences(device, 1, &frames[frameNum].fence, VK_TRUE, UINT64_MAX));

    uint32_t swapIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
        frames[frameNum].acquireSemaphore, VK_NULL_HANDLE, &swapIndex);
    boolean resize = false;
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        VK_RecreateSwapchain();
        columnCount = 0;
        return;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR)
    {
        resize = true;
    }

    vk_swapimage_t* swapImage = &swapImages[swapIndex];

    VK_CHECK(vkResetFences(device, 1, &frames[frameNum].fence));
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo beginInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkImageMemoryBarrier2 clearBarrier = VK_ImageBarrier(frameIndexed.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_CLEAR_BIT);

    VkDependencyInfo clearDepInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &clearBarrier
    };

    vkCmdPipelineBarrier2(cmd, &clearDepInfo);

    VkClearColorValue clear =
    {
        .uint32 = {0, 0, 0, 0}
    };

    VkImageSubresourceRange clearRange =
    {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1
    };

    vkCmdClearColorImage(cmd, frameIndexed.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &clearRange);

    vkCmdPushConstants(cmd, transferLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(VkDeviceAddress), &columnBuffer.address);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        transferLayout, 0, 1, &transferDescSet, 0, NULL);

    VkImageMemoryBarrier2 drawBarrier = VK_ImageBarrier(frameIndexed.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    VkDependencyInfo drawDepInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &drawBarrier
    };

    vkCmdPipelineBarrier2(cmd, &drawDepInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, drawPipeline);

    uint32_t xGroups = (columnCount + 127) / 128;
    vkCmdDispatch(cmd, xGroups, 1, 1);

    VkImageMemoryBarrier2 transferBarriers[2];
    transferBarriers[0] = VK_ImageBarrier(frameIndexed.image,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    transferBarriers[1] = VK_ImageBarrier(frameColor.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    VkDependencyInfo transferDepInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = transferBarriers
    };

    vkCmdPipelineBarrier2(cmd, &transferDepInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, transferPipeline);

    vkCmdPushConstants(cmd, transferLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(VkDeviceAddress), &paletteBuffer.address);

    xGroups = (frameWidth + 15) / 16;
    uint32_t yGroups = (frameHeight + 15) / 16;
    vkCmdDispatch(cmd, xGroups, yGroups, 1);

    VkImageMemoryBarrier2 blitBarriers[2];
    blitBarriers[0] = VK_ImageBarrier(frameColor.image,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT);
    blitBarriers[1] = VK_ImageBarrier(swapImage->image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT);

    VkDependencyInfo blitDepInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = blitBarriers
    };

    VkImageBlit2 blit =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .srcSubresource =
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1
        },
        .srcOffsets =
        {
            {0, 0, 0},
            {frameWidth, frameHeight, 1}
        },
        .dstSubresource =
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1
        },
        .dstOffsets =
        {
            {0, 0, 0},
            {swapWidth, swapHeight, 1}
        },
    };

    VkBlitImageInfo2 blitInfo =
    {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .srcImage = frameColor.image,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = swapImage->image,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blit,
        .filter = VK_FILTER_NEAREST
    };

    vkCmdPipelineBarrier2(cmd, &blitDepInfo);
    vkCmdBlitImage2(cmd, &blitInfo);

    VkImageMemoryBarrier2 presentBarrier = VK_ImageBarrier(swapImage->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    VkDependencyInfo presentDepInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &presentBarrier
    };

    vkCmdPipelineBarrier2(cmd, &presentDepInfo);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit =
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames[frameNum].acquireSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &swapImage->renderSemaphore
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, frames[frameNum].fence));

    VkPresentInfoKHR presentInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &swapImage->renderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &swapIndex
    };

    VkResult presentResult = vkQueuePresentKHR(queue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || resize)
    {
        resize = false;
        VK_RecreateSwapchain();
    }

    frameNum++;
    frameNum %= FRAMES_IN_FLIGHT;
    columnCount = 0;
}

