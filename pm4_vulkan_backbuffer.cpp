#include "pm4_vulkan_backbuffer.h"

#include <array>
#include <cstdio>

PM4VulkanBackbuffer::~PM4VulkanBackbuffer()
{
    Destroy();
}

bool PM4VulkanBackbuffer::Initialize(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue graphicsQueue,
    uint32_t graphicsQueueFamily,
    VkFormat colorFormat,
    VkExtent2D extent,
    VkSampleCountFlagBits samples)
{
    if (physicalDevice == VK_NULL_HANDLE ||
        device == VK_NULL_HANDLE ||
        graphicsQueue == VK_NULL_HANDLE)
    {
        return false;
    }

    if (extent.width == 0 || extent.height == 0)
        return false;

    if (samples != VK_SAMPLE_COUNT_1_BIT)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Only VK_SAMPLE_COUNT_1_BIT is currently supported\n");
        return false;
    }

    m_physicalDevice = physicalDevice;
    m_device = device;
    m_graphicsQueue = graphicsQueue;
    m_graphicsQueueFamily = graphicsQueueFamily;
    m_colorFormat = colorFormat;
    m_extent = extent;
    m_samples = samples;

    m_colorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (!CreateColorImage())
        return false;

    if (!CreateDepthImage())
        return false;

    if (!CreateImageViews())
        return false;

    if (!CreateRenderPass())
        return false;

    if (!CreateFramebuffer())
        return false;

    if (!CreateCommandPool())
        return false;

    if (!CreateCommandBuffer())
        return false;

    return true;
}

void PM4VulkanBackbuffer::Destroy()
{
    if (m_device == VK_NULL_HANDLE)
        return;

    vkDeviceWaitIdle(m_device);

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(
            m_device,
            m_commandPool,
            nullptr);

        m_commandPool = VK_NULL_HANDLE;
        m_commandBuffer = VK_NULL_HANDLE;
    }

    if (m_framebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(
            m_device,
            m_framebuffer,
            nullptr);

        m_framebuffer = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(
            m_device,
            m_renderPass,
            nullptr);

        m_renderPass = VK_NULL_HANDLE;
    }

    if (m_colorView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(
            m_device,
            m_colorView,
            nullptr);

        m_colorView = VK_NULL_HANDLE;
    }

    if (m_depthView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(
            m_device,
            m_depthView,
            nullptr);

        m_depthView = VK_NULL_HANDLE;
    }

    if (m_colorImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(
            m_device,
            m_colorImage,
            nullptr);

        m_colorImage = VK_NULL_HANDLE;
    }

    if (m_depthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(
            m_device,
            m_depthImage,
            nullptr);

        m_depthImage = VK_NULL_HANDLE;
    }

    if (m_colorMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(
            m_device,
            m_colorMemory,
            nullptr);

        m_colorMemory = VK_NULL_HANDLE;
    }

    if (m_depthMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(
            m_device,
            m_depthMemory,
            nullptr);

        m_depthMemory = VK_NULL_HANDLE;
    }

    m_physicalDevice = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;

    m_graphicsQueueFamily = 0;

    m_colorFormat = VK_FORMAT_UNDEFINED;
    m_depthFormat = VK_FORMAT_UNDEFINED;

    m_extent = {};
    m_samples = VK_SAMPLE_COUNT_1_BIT;

    m_colorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    m_recording = false;
    m_rendering = false;
}

bool PM4VulkanBackbuffer::CreateColorImage()
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_colorFormat;
    imageInfo.extent.width = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = m_samples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(
        m_device,
        &imageInfo,
        nullptr,
        &m_colorImage);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to create color image: %d\n",
            result);
        return false;
    }

    if (!AllocateImageMemory(
            m_colorImage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_colorMemory))
    {
        return false;
    }

    return true;
}

bool PM4VulkanBackbuffer::CreateDepthImage()
{
    m_depthFormat = FindDepthFormat();

    if (m_depthFormat == VK_FORMAT_UNDEFINED)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to find depth format\n");
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_depthFormat;
    imageInfo.extent.width = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = m_samples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(
        m_device,
        &imageInfo,
        nullptr,
        &m_depthImage);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to create depth image: %d\n",
            result);
        return false;
    }

    if (!AllocateImageMemory(
            m_depthImage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_depthMemory))
    {
        return false;
    }

    return true;
}

bool PM4VulkanBackbuffer::CreateImageViews()
{
    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType =
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image = m_colorImage;
    colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format = m_colorFormat;
    colorViewInfo.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(
        m_device,
        &colorViewInfo,
        nullptr,
        &m_colorView);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to create color image view: %d\n",
            result);
        return false;
    }

    VkImageAspectFlags depthAspect =
        VK_IMAGE_ASPECT_DEPTH_BIT;

    if (m_depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        m_depthFormat == VK_FORMAT_D24_UNORM_S8_UINT)
    {
        depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType =
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = m_depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = m_depthFormat;
    depthViewInfo.subresourceRange.aspectMask =
        depthAspect;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(
        m_device,
        &depthViewInfo,
        nullptr,
        &m_depthView);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to create depth image view: %d\n",
            result);
        return false;
    }

    return true;
}

bool PM4VulkanBackbuffer::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_colorFormat;
    colorAttachment.samples = m_samples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout =
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = m_samples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 1;
    depthReference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask =
        VK_ACCESS_TRANSFER_READ_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments =
    {
        colorAttachment,
        depthAttachment
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType =
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount =
        static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(
        m_device,
        &renderPassInfo,
        nullptr,
        &m_renderPass);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to create render pass: %d\n",
            result);
        return false;
    }

    return true;
}

bool PM4VulkanBackbuffer::CreateFramebuffer()
{
    std::array<VkImageView, 2> attachments =
    {
        m_colorView,
        m_depthView
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType =
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount =
        static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = m_extent.width;
    framebufferInfo.height = m_extent.height;
    framebufferInfo.layers = 1;

    VkResult result = vkCreateFramebuffer(
        m_device,
        &framebufferInfo,
        nullptr,
        &m_framebuffer);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to create framebuffer: %d\n",
            result);
        return false;
    }

    return true;
}

bool PM4VulkanBackbuffer::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex =
        m_graphicsQueueFamily;

    VkResult result = vkCreateCommandPool(
        m_device,
        &poolInfo,
        nullptr,
        &m_commandPool);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to create command pool: %d\n",
            result);
        return false;
    }

    return true;
}

bool PM4VulkanBackbuffer::CreateCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level =
        VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkResult result = vkAllocateCommandBuffers(
        m_device,
        &allocInfo,
        &m_commandBuffer);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to allocate command buffer: %d\n",
            result);
        return false;
    }

    return true;
}

bool PM4VulkanBackbuffer::AllocateImageMemory(
    VkImage image,
    VkMemoryPropertyFlags properties,
    VkDeviceMemory& memory)
{
    VkMemoryRequirements requirements{};

    vkGetImageMemoryRequirements(
        m_device,
        image,
        &requirements);

    uint32_t memoryType =
        FindMemoryType(
            requirements.memoryTypeBits,
            properties);

    if (memoryType == UINT32_MAX)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] No suitable image memory type found\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType =
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize =
        requirements.size;
    allocInfo.memoryTypeIndex =
        memoryType;

    VkResult result = vkAllocateMemory(
        m_device,
        &allocInfo,
        nullptr,
        &memory);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to allocate image memory: %d\n",
            result);
        return false;
    }

    result = vkBindImageMemory(
        m_device,
        image,
        memory,
        0);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to bind image memory: %d\n",
            result);

        vkFreeMemory(
            m_device,
            memory,
            nullptr);

        memory = VK_NULL_HANDLE;

        return false;
    }

    return true;
}

uint32_t PM4VulkanBackbuffer::FindMemoryType(
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};

    vkGetPhysicalDeviceMemoryProperties(
        m_physicalDevice,
        &memoryProperties);

    for (uint32_t i = 0;
         i < memoryProperties.memoryTypeCount;
         ++i)
    {
        if ((typeFilter & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags &
             properties) == properties)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

VkFormat PM4VulkanBackbuffer::FindDepthFormat() const
{
    const VkFormat candidates[] =
    {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates)
    {
        VkFormatProperties properties{};

        vkGetPhysicalDeviceFormatProperties(
            m_physicalDevice,
            format,
            &properties);

        if (properties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

bool PM4VulkanBackbuffer::BeginCommandBuffer()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult result = vkBeginCommandBuffer(
        m_commandBuffer,
        &beginInfo);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to begin command buffer: %d\n",
            result);
        return false;
    }

    m_recording = true;
    return true;
}

bool PM4VulkanBackbuffer::BeginFrame()
{
    if (m_device == VK_NULL_HANDLE ||
        m_commandBuffer == VK_NULL_HANDLE)
    {
        return false;
    }

    if (m_recording || m_rendering)
        return false;

    VkResult result = vkResetCommandBuffer(
        m_commandBuffer,
        0);

    if (result != VK_SUCCESS)
        return false;

    if (!BeginCommandBuffer())
        return false;

    if (m_colorLayout !=
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        TransitionImage(
            m_commandBuffer,
            m_colorImage,
            m_colorLayout,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT);

        m_colorLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (m_depthLayout !=
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        TransitionImage(
            m_commandBuffer,
            m_depthImage,
            m_depthLayout,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        m_depthLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    return true;
}

bool PM4VulkanBackbuffer::BeginRendering(
    bool clearColor,
    bool clearDepth)
{
    if (!m_recording || m_rendering)
        return false;

    VkClearValue clearValues[2]{};

    clearValues[0].color.float32[0] = 0.0f;
    clearValues[0].color.float32[1] = 0.0f;
    clearValues[0].color.float32[2] = 0.0f;
    clearValues[0].color.float32[3] = 1.0f;

    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType =
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_extent;

    /*
     * The render pass uses LOAD operations.
     * If the caller requests a clear, perform it explicitly
     * after beginning the render pass.
     */
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(
        m_commandBuffer,
        &renderPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    m_rendering = true;

    VkClearAttachment attachments[2]{};
    uint32_t attachmentCount = 0;

    if (clearColor)
    {
        attachments[attachmentCount].aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        attachments[attachmentCount].colorAttachment = 0;
        attachments[attachmentCount].clearValue =
            clearValues[0];

        ++attachmentCount;
    }

    if (clearDepth)
    {
        attachments[attachmentCount].aspectMask =
            VK_IMAGE_ASPECT_DEPTH_BIT;
        attachments[attachmentCount].clearValue =
            clearValues[1];

        ++attachmentCount;
    }

    if (attachmentCount != 0)
    {
        VkClearRect clearRect{};
        clearRect.rect.offset = {0, 0};
        clearRect.rect.extent = m_extent;
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;

        vkCmdClearAttachments(
            m_commandBuffer,
            attachmentCount,
            attachments,
            1,
            &clearRect);
    }

    return true;
}

bool PM4VulkanBackbuffer::EndRendering()
{
    if (!m_rendering)
        return false;

    vkCmdEndRenderPass(m_commandBuffer);

    m_rendering = false;

    m_colorLayout =
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    m_depthLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    return true;
}

bool PM4VulkanBackbuffer::CopyToSwapchain(
    VkImage swapchainImage,
    VkExtent2D swapchainExtent,
    VkImageLayout oldSwapchainLayout)
{
    if (!m_recording ||
        m_rendering ||
        swapchainImage == VK_NULL_HANDLE)
    {
        return false;
    }

    if (m_colorLayout !=
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        TransitionImage(
            m_commandBuffer,
            m_colorImage,
            m_colorLayout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT);

        m_colorLayout =
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    TransitionImage(
        m_commandBuffer,
        swapchainImage,
        oldSwapchainLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageCopy copyRegion{};

    copyRegion.srcSubresource.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.layerCount = 1;

    copyRegion.dstSubresource.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.layerCount = 1;

    copyRegion.srcOffset = {0, 0, 0};
    copyRegion.dstOffset = {0, 0, 0};

    copyRegion.extent.width =
        (m_extent.width < swapchainExtent.width)
            ? m_extent.width
            : swapchainExtent.width;

    copyRegion.extent.height =
        (m_extent.height < swapchainExtent.height)
            ? m_extent.height
            : swapchainExtent.height;

    copyRegion.extent.depth = 1;

    vkCmdCopyImage(
        m_commandBuffer,
        m_colorImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion);

    TransitionImage(
        m_commandBuffer,
        swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_ASPECT_COLOR_BIT);

    return true;
}

bool PM4VulkanBackbuffer::EndFrame()
{
    if (!m_recording || m_rendering)
        return false;

    VkResult result = vkEndCommandBuffer(
        m_commandBuffer);

    if (result != VK_SUCCESS)
    {
        std::fprintf(
            stderr,
            "[Backbuffer] Failed to end command buffer: %d\n",
            result);
        return false;
    }

    m_recording = false;
    return true;
}

void PM4VulkanBackbuffer::TransitionImage(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspectMask)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType =
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;

    barrier.subresourceRange.aspectMask =
        aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    VkPipelineStageFlags dstStage =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        barrier.srcAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    else if (oldLayout ==
             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.srcAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout ==
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        srcStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout ==
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask =
            VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage =
            VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout ==
             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        barrier.srcAccessMask =
            VK_ACCESS_TRANSFER_READ_BIT;

        srcStage =
            VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (newLayout ==
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        dstStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (newLayout ==
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        dstStage =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (newLayout ==
             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        barrier.dstAccessMask =
            VK_ACCESS_TRANSFER_READ_BIT;

        dstStage =
            VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (newLayout ==
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.dstAccessMask =
            VK_ACCESS_TRANSFER_WRITE_BIT;

        dstStage =
            VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (newLayout ==
             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.dstAccessMask = 0;

        dstStage =
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage,
        dstStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);
}

VkImage PM4VulkanBackbuffer::GetColorImage() const
{
    return m_colorImage;
}

VkImageView PM4VulkanBackbuffer::GetColorImageView() const
{
    return m_colorView;
}

VkImage PM4VulkanBackbuffer::GetDepthImage() const
{
    return m_depthImage;
}

VkImageView PM4VulkanBackbuffer::GetDepthImageView() const
{
    return m_depthView;
}

VkFormat PM4VulkanBackbuffer::GetColorFormat() const
{
    return m_colorFormat;
}

VkFormat PM4VulkanBackbuffer::GetDepthFormat() const
{
    return m_depthFormat;
}

VkExtent2D PM4VulkanBackbuffer::GetExtent() const
{
    return m_extent;
}

VkRenderPass PM4VulkanBackbuffer::GetRenderPass() const
{
    return m_renderPass;
}

VkFramebuffer PM4VulkanBackbuffer::GetFramebuffer() const
{
    return m_framebuffer;
}

VkCommandPool PM4VulkanBackbuffer::GetCommandPool() const
{
    return m_commandPool;
}

VkCommandBuffer PM4VulkanBackbuffer::GetCommandBuffer() const
{
    return m_commandBuffer;
}
