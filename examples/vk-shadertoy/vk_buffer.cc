
#include "vk_buffer.h"

#include "vk_common.h"


VulkanBuffer::VulkanBuffer(const vk::DispatchLoaderDynamic &dispatcher, VkDevice device,
                           const VkPhysicalDeviceMemoryProperties &phy_dev_mem_props, uint32_t size,
                           VkBufferUsageFlagBits usage)
        : d_(dispatcher), device_(device), phy_dev_mem_props_(phy_dev_mem_props),
          size_(size), usage_(usage), host_visible_(false) {
}

VulkanBuffer::~VulkanBuffer() {
}

vk::Result VulkanBuffer::create() {

    buffer_ = nullptr;
    buffer_mem_ = nullptr;
    view_ = nullptr;

    bool shared = sharing_queue_count_ > 1;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size_ * sizeof(float);
    buffer_info.usage = usage_;
    buffer_info.sharingMode = shared ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.queueFamilyIndexCount = shared ? sharing_queue_count_ : 0;
    buffer_info.pQueueFamilyIndices = shared ? sharing_queues_ : nullptr;
    CHECK_VK_RESULT(d_.vkCreateBuffer(device_, &buffer_info, nullptr, &buffer_));

    VkMemoryRequirements mem_req{};
    d_.vkGetBufferMemoryRequirements(device_, buffer_, &mem_req);

    uint32_t mem_index = find_memory(phy_dev_mem_props_, mem_req,
                                     host_visible_ ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                                   : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_index >= phy_dev_mem_props_.memoryTypeCount)
        return vk::Result::eErrorMemoryMapFailed;

    VkMemoryAllocateInfo mem_info{};
    mem_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_info.allocationSize = mem_req.size;
    mem_info.memoryTypeIndex = mem_index;

    CHECK_VK_RESULT(d_.vkAllocateMemory(device_, &mem_info, nullptr, &buffer_mem_));
    CHECK_VK_RESULT(d_.vkBindBufferMemory(device_, buffer_, buffer_mem_, 0));

    if (make_view_) {
        if ((usage_ & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) ||
            (usage_ & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)) {
            VkBufferViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
            view_info.buffer = buffer_;
            view_info.format = format_;
            view_info.offset = 0;
            view_info.range = VK_WHOLE_SIZE;
            CHECK_VK_RESULT(d_.vkCreateBufferView(device_, &view_info, nullptr, &view_));
        }
    }

    return vk::Result::eSuccess;
}
