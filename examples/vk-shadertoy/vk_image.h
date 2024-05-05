
#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS 1
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

class VulkanImage {
public:
    VulkanImage(const vk::DispatchLoaderDynamic &dispatcher, const VkPhysicalDevice &physical_device,
                const VkDevice &device,
                const VkPhysicalDeviceFeatures &physical_device_features,
                const VkPhysicalDeviceProperties &physical_device_properties,
                const VkPhysicalDeviceMemoryProperties &physical_device_memory_properties);

    ~VulkanImage();

    void Create();

private:
    const vk::DispatchLoaderDynamic &d_;
    struct {
        const VkPhysicalDevice &device;
        const VkPhysicalDeviceFeatures &features;
        const VkPhysicalDeviceProperties &properties;
        const VkPhysicalDeviceMemoryProperties &memory_properties;
    } physical_;

    VkDevice device_;

    VkFormat format_;
    VkExtent2D extent_{};
    VkImageUsageFlagBits usage_;
    bool make_view_{};
    bool will_be_initialized_{};
    bool host_visible_{};
    bool multisample_{};
    uint32_t *sharing_queues_{};
    uint32_t sharing_queue_count_{};
    VkImage image_{};
    VkDeviceMemory image_mem_{};
    VkImageView view_{};
    VkSampler sampler_{};
    bool anisotropyEnable_{};
    VkSamplerAddressMode repeat_mode_;
    bool mipmaps_{};
    bool linear_{};
};