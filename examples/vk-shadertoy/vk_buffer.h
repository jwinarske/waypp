
#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS 1
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

class VulkanBuffer {
public:
    VulkanBuffer(const vk::DispatchLoaderDynamic &dispatcher, VkDevice device,
                 const VkPhysicalDeviceMemoryProperties &physical_device_memory_properties, uint32_t size,
                 VkBufferUsageFlagBits usage);

    ~VulkanBuffer();

    vk::Result create();

private:
    const vk::DispatchLoaderDynamic &d_;
    const VkPhysicalDeviceMemoryProperties &phy_dev_mem_props_;
    VkDevice device_;

    VkFormat format_;
    uint32_t size_;

    VkBufferUsageFlagBits usage_;

    bool make_view_{};
    bool host_visible_;

    uint32_t *sharing_queues_{};
    uint32_t sharing_queue_count_{};

    VkBuffer buffer_{};
    VkDeviceMemory buffer_mem_{};
    VkBufferView view_{};
};
