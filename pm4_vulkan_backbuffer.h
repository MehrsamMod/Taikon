#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class PM4VulkanBackbuffer
{
public:
    PM4VulkanBackbuffer() = default;
    ~PM4VulkanBackbuffer();

    PM4VulkanBackbuffer(const PM4VulkanBackbuffer&) = delete;
    PM4VulkanBackbuffer& operator=(const PM4VulkanBackbuffer&) = delete;

    bool Initialize(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkQueue graphicsQueue,
        uint32_t graphicsQueueFamily,
        VkFormat colorFormat,
        VkExtent2D extent,
        VkSampleCountFlagBits samples =
            VK_SAMPLE_COUNT_1_BIT);

    void Destroy();

    bool BeginFrame();

    bool BeginRendering(
        bool clearColor = true,
        bool clearDepth = true);

    bool EndRendering();

    bool CopyToSwapchain(
        VkImage swapchainImage,
        VkExtent2D swapchainExtent,
        VkImageLayout oldSwapchainLayout =
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    bool EndFrame();

    VkImage GetColorImage() const;
    VkImageView GetColorImageView() const;

    VkImage GetDepthImage() const;
    VkImageView GetDepthImageView() const;

    VkFormat GetColorFormat() const;
    VkFormat GetDepthFormat() const;

    VkExtent2D GetExtent() const;

    VkRenderPass GetRenderPass() const;
    VkFramebuffer GetFramebuffer() const;

    VkCommandPool GetCommandPool() const;
    VkCommandBuffer GetCommandBuffer() const;

private:
    bool CreateColorImage();
    bool CreateDepthImage();
    bool CreateImageViews();
    bool CreateRenderPass();
    bool CreateFramebuffer();
    bool CreateCommandPool();
    bool CreateCommandBuffer();

    bool AllocateImageMemory(
        VkImage image,
        VkMemoryPropertyFlags properties,
        VkDeviceMemory& memory);

    uint32_t FindMemoryType(
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties) const;

    VkFormat FindDepthFormat() const;

    bool BeginCommandBuffer();

    void TransitionImage(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask);

private:
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    uint32_t m_graphicsQueueFamily = 0;

    VkFormat m_colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;

    VkExtent2D m_extent{};

    VkSampleCountFlagBits m_samples =
        VK_SAMPLE_COUNT_1_BIT;

    VkImage m_colorImage = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMemory = VK_NULL_HANDLE;
    VkImageView m_colorView = VK_NULL_HANDLE;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;
    VkImageView m_depthView = VK_NULL_HANDLE;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

    VkImageLayout m_colorLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageLayout m_depthLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    bool m_recording = false;
    bool m_rendering = false;
};
