
#include "vk_image.h"

#include "vk_common.h"


VulkanImage::VulkanImage(const vk::DispatchLoaderDynamic &dispatcher, const VkPhysicalDevice &physical_device,
                         const VkDevice &device,
                         const VkPhysicalDeviceFeatures &physical_device_features,
                         const VkPhysicalDeviceProperties &physical_device_properties,
                         const VkPhysicalDeviceMemoryProperties &physical_device_memory_properties) :
        d_(dispatcher),
        device_(device),
        physical_({
                          .device = physical_device,
                          .features = physical_device_features,
                          .properties = physical_device_properties,
                          .memory_properties = physical_device_memory_properties,
                  }) {
}

VulkanImage::~VulkanImage() {

}

void VulkanImage::Create() {
    VkResult res;

    image_ = nullptr;
    image_mem_ = nullptr;
    view_ = nullptr;
    sampler_ = nullptr;

    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (will_be_initialized_ || host_visible_) {
        usage_ = static_cast<VkImageUsageFlagBits>(usage_ & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT));
        layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        tiling = VK_IMAGE_TILING_LINEAR;
    } else if (multisample_) {
        VkImageFormatProperties format_properties;
        res = d_.vkGetPhysicalDeviceImageFormatProperties(physical_.device, format_, VK_IMAGE_TYPE_2D, tiling, usage_,
                                                          0, &format_properties);
        if (res != VK_SUCCESS) {
            for (uint32_t s = VK_SAMPLE_COUNT_16_BIT; s != 0; s >>= 1)
                if ((format_properties.sampleCounts & s)) {
                    samples = static_cast<VkSampleCountFlagBits>(s);
                    break;
                }
        }
    }

    uint32_t mipLevels = 1;
    if (mipmaps_) {
        mipLevels = static_cast<uint32_t>((log(std::max(extent_.width, extent_.height)) / log(2)) + 1);
    }

    bool shared = sharing_queue_count_ > 1;
    struct VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format_;
    image_info.extent = {extent_.width, extent_.height, 1};
    image_info.mipLevels = mipLevels;
    image_info.arrayLayers = 1;
    image_info.samples = samples;
    image_info.tiling = tiling;
    image_info.usage = usage_;
    image_info.sharingMode = shared ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = shared ? sharing_queue_count_ : 0;
    image_info.pQueueFamilyIndices = shared ? sharing_queues_ : nullptr;
    image_info.initialLayout = layout;

    res = d_.vkCreateImage(device_, &image_info, nullptr, &image_);
    if (res != VK_SUCCESS)
        return;

    VkMemoryRequirements mem_req{};
    d_.vkGetImageMemoryRequirements(device_, image_, &mem_req);
    uint32_t mem_index = find_memory(physical_.memory_properties, mem_req, host_visible_ ?
                                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT :
                                                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_index >= physical_.memory_properties.memoryTypeCount)
        return;

    VkMemoryAllocateInfo mem_info{};
    mem_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_info.allocationSize = mem_req.size;
    mem_info.memoryTypeIndex = mem_index;
    res = d_.vkAllocateMemory(device_, &mem_info, nullptr, &image_mem_);
    if (res != VK_SUCCESS)
        return;

    res = d_.vkBindImageMemory(device_, image_, image_mem_, 0);
    if (res != VK_SUCCESS)
        return;

    if (make_view_) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image_;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format_;
        view_info.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                VK_COMPONENT_SWIZZLE_A,};
        view_info.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(
                (usage_ & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0 ?
                VK_IMAGE_ASPECT_COLOR_BIT :
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        res = d_.vkCreateImageView(device_, &view_info, nullptr, &view_);
        if (res != VK_SUCCESS)
            return;
    }

    if ((usage_ & VK_IMAGE_USAGE_SAMPLED_BIT)) {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = linear_ ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler_info.minFilter = linear_ ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = repeat_mode_;
        sampler_info.addressModeV = repeat_mode_;
        sampler_info.addressModeW = repeat_mode_;
        sampler_info.anisotropyEnable = anisotropyEnable_ && physical_.features.samplerAnisotropy;
        sampler_info.maxAnisotropy = physical_.properties.limits.maxSamplerAnisotropy;
        sampler_info.minLod = 0;
        sampler_info.maxLod = 1;

        if (mipmaps_) {
            sampler_info.maxLod = static_cast<float>(mipLevels);
            sampler_info.mipLodBias = 0;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        }

        res = d_.vkCreateSampler(device_, &sampler_info, nullptr, &sampler_);
        if (res != VK_SUCCESS)
            return;
    }
}
