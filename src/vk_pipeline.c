#include <stdio.h>
#include <stdlib.h>
#include "volk.h"

#include "vk_main.h"
#include "vk_pipeline.h"

#include "shader_transfer.h"
#include "shader_draw.h"

VkPipeline CreateComputePipeline(VkPipelineLayout layout,
    const uint32_t* shaderData, size_t shaderSize)
{
    VkShaderModuleCreateInfo shaderInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderSize,
        .pCode = shaderData
    };
    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &shaderInfo, NULL, &module));

    VkPipelineShaderStageCreateInfo stageInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main"
    };

    VkComputePipelineCreateInfo pipelineInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stageInfo,
        .layout = layout
    };

    VkPipeline result;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &result));
    vkDestroyShaderModule(device, module, NULL);
    return result;
}

void VK_InitPipelines()
{
    VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2}};

    VkDescriptorPoolCreateInfo descPoolInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = poolSizes
    };

    VK_CHECK(vkCreateDescriptorPool(device, &descPoolInfo, NULL, &descPool));

    VkDescriptorSetLayoutBinding bindings[2] =
    {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
    };

    VkDescriptorSetLayoutCreateInfo descLayoutInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(device, &descLayoutInfo, NULL, &transferDescLayout));

    VkDescriptorSetAllocateInfo descAllocInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &transferDescLayout
    };

    VK_CHECK(vkAllocateDescriptorSets(device, &descAllocInfo, &transferDescSet));

    VkPushConstantRange transferConsts =
    {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = sizeof(vk_pushconst_t)
    };

    VkPipelineLayoutCreateInfo transferLayoutInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &transferDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &transferConsts
    };

    VK_CHECK(vkCreatePipelineLayout(device, &transferLayoutInfo, NULL, &transferLayout));

    transferPipeline = CreateComputePipeline(transferLayout, shader_transfer, sizeof(shader_transfer));
    drawPipeline = CreateComputePipeline(transferLayout, shader_draw, sizeof(shader_draw));
}