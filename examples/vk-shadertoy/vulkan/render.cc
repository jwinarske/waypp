/*
 * Copyright © 2024 Joel Winarske
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// Danil, 2021+ Vulkan shader launcher, self https://github.com/danilw/vulkan-shadertoy-launcher
// The MIT License

#include "render.h"

#include <cmath>

#include "logging.h"
#include "utils.h"

#define d VULKAN_HPP_DEFAULT_DISPATCHER

class VulkanUtils;


VulkanRender::VulkanRender() = default;

VulkanRender::~VulkanRender() = default;

int VulkanRender::get_essentials(struct vk_render_essentials *essentials, struct vk_physical_device *phy_dev,
                                 struct vk_device *dev, struct vk_swapchain *swapchain) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    essentials->images = VulkanUtils::get_swapchain_images(dev, swapchain, &essentials->image_count);
    if (essentials->images == nullptr)
        return -1;

    uint32_t *presentable_queues = nullptr;
    uint32_t presentable_queue_count = 0;

    retval = VulkanUtils::get_presentable_queues(phy_dev, dev, swapchain->surface, &presentable_queues,
                                                 &presentable_queue_count);
    if (!vk_error_is_success(&retval) || presentable_queue_count == 0) {
        spdlog::error("No presentable queue families!  What kind of graphics card is this!");
        return -1;
    }

    essentials->present_queue = dev->command_pools[presentable_queues[0]].queues[0];
    essentials->cmd_buffer = dev->command_pools[presentable_queues[0]].buffers[0];
    free(presentable_queues);

    VkSemaphoreCreateInfo sem_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    res = d.vkCreateSemaphore(dev->device, &sem_info, nullptr,
                              &essentials->sem_post_acquire);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to create post-acquire semaphore\n");
        return -1;
    }

    res = d.vkCreateSemaphore(dev->device, &sem_info, nullptr, &essentials->sem_pre_submit);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to create pre-submit semaphore\n");
        return -1;
    }

    VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    res = d.vkCreateFence(dev->device, &fence_info, nullptr, &essentials->exec_fence);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to create fence\n");
        return -1;
    }

    essentials->first_render = true;

    return 0;
}

void VulkanRender::cleanup_essentials(struct vk_render_essentials *essentials, struct vk_device *dev) {
    d.vkDeviceWaitIdle(dev->device);

    d.vkDestroySemaphore(dev->device, essentials->sem_post_acquire, nullptr);
    d.vkDestroySemaphore(dev->device, essentials->sem_pre_submit, nullptr);
    d.vkDestroyFence(dev->device, essentials->exec_fence, nullptr);
    free(essentials->images);
}

VkResult VulkanRender::start(struct vk_render_essentials *essentials, struct vk_device *dev,
                             struct vk_swapchain *swapchain, VkImageLayout to_layout, uint32_t *image_index) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    res = d.vkAcquireNextImageKHR(dev->device, swapchain->swapchain, 1000000000,
                                  essentials->sem_post_acquire, nullptr,
                                  image_index);
    vk_error_set_vkresult(&retval, res);
    if (res == VK_TIMEOUT) {
        spdlog::warn("A whole second and no image.  I give up.");
        return res;
    } else if (res == VK_SUBOPTIMAL_KHR)
        spdlog::warn("presentation is suboptimal.");
    else if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        // this is not error, this is resize event for AMD hardware
        return res;
    } else if (res < 0) {
        vk_error_printf(&retval, "Couldn't acquire image\n");
        return res;
    }

    if (!essentials->first_render) {
        res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
        vk_error_set_vkresult(&retval, res);
        if (res) {
            vk_error_printf(&retval, "Wait for fence failed\n");
            return res;
        }
    }
    essentials->first_render = false;

    d.vkResetCommandBuffer(essentials->cmd_buffer, 0);
    VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    res = d.vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Couldn't even begin recording a command buffer\n");
        return res;
    }

    VkImageMemoryBarrier image_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = to_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = essentials->images[*image_index],
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            },
    };

    d.vkCmdPipelineBarrier(essentials->cmd_buffer,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                           0,
                           0, nullptr,
                           0, nullptr,
                           1, &image_barrier);

    return VK_SUCCESS;
}


vk_error
VulkanRender::fill_object(struct vk_device *dev, VkDeviceMemory to, void *from, size_t size, const char *object,
                          const char *name) {
    void *mem = nullptr;
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    res = d.vkMapMemory(dev->device, to, 0, size, 0, &mem);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to map memory of the %s %s\n", name, object);
        return retval;
    }

    memcpy(mem, from, size);

    d.vkUnmapMemory(dev->device, to);

    return retval;
}

vk_error VulkanRender::fill_buffer(struct vk_device *dev, struct vk_buffer *to, void *from, size_t size,
                                   const char *name) {
    return fill_object(dev, to->buffer_mem, from, size, "buffer", name);
}

vk_error VulkanRender::fill_image(struct vk_device *dev, struct vk_image *to, void *from, size_t size,
                                  const char *name) {
    return fill_object(dev, to->image_mem, from, size, "image", name);
}

vk_error
VulkanRender::copy_object_start(struct vk_device * /* dev */, struct vk_render_essentials *essentials,
                                const char *object,
                                const char *name) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    d.vkResetCommandBuffer(essentials->cmd_buffer, 0);
    VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    res = d.vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
    vk_error_set_vkresult(&retval, res);
    if (res)
        vk_error_printf(&retval, "Couldn't begin recording a command buffer to copy the %s %s\n", name, object);

    return retval;
}

vk_error VulkanRender::copy_object_end(struct vk_device *dev, struct vk_render_essentials *essentials) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    d.vkEndCommandBuffer(essentials->cmd_buffer);

    res = d.vkResetFences(dev->device, 1, &essentials->exec_fence);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return retval;
    }

    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &essentials->cmd_buffer,
    };

    d.vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);
    res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
    vk_error_set_vkresult(&retval, res);

    return retval;
}

vk_error VulkanRender::copy_buffer(struct vk_device *dev, struct vk_render_essentials *essentials,
                                   struct vk_buffer *to, struct vk_buffer *from, size_t size, const char *name) {
    vk_error retval = VK_ERROR_NONE;

    retval = copy_object_start(dev, essentials, "buffer", name);
    if (!vk_error_is_success(&retval))
        return retval;

    VkBufferCopy copy_region = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
    };
    d.vkCmdCopyBuffer(essentials->cmd_buffer, from->buffer, to->buffer, 1, &copy_region);

    return copy_object_end(dev, essentials);
}

vk_error VulkanRender::copy_image(struct vk_device *dev, struct vk_render_essentials *essentials,
                                  struct vk_image *to, VkImageLayout to_layout, struct vk_image *from,
                                  VkImageLayout from_layout,
                                  VkImageCopy *region, const char *name) {
    vk_error retval = VK_ERROR_NONE;

    retval = copy_object_start(dev, essentials, "image", name);
    if (!vk_error_is_success(&retval))
        return retval;

    d.vkCmdCopyImage(essentials->cmd_buffer, from->image, from_layout, to->image, to_layout,
                     1, region);

    return copy_object_end(dev, essentials);
}

vk_error VulkanRender::copy_buffer_to_image(struct vk_device *dev, struct vk_render_essentials *essentials,
                                            struct vk_image *to, VkImageLayout to_layout, struct vk_buffer *from,
                                            VkBufferImageCopy *region, const char *name) {
    vk_error retval = VK_ERROR_NONE;

    retval = copy_object_start(dev, essentials, "image", name);
    if (!vk_error_is_success(&retval))
        return retval;

    d.vkCmdCopyBufferToImage(essentials->cmd_buffer, from->buffer, to->image, to_layout, 1,
                             region);

    return copy_object_end(dev, essentials);
}

vk_error VulkanRender::copy_image_to_buffer(struct vk_device *dev, struct vk_render_essentials *essentials,
                                            struct vk_buffer *to, struct vk_image *from, VkImageLayout from_layout,
                                            VkBufferImageCopy *region, const char *name) {
    vk_error retval = VK_ERROR_NONE;

    retval = copy_object_start(dev, essentials, "buffer", name);
    if (!vk_error_is_success(&retval))
        return retval;

    d.vkCmdCopyImageToBuffer(essentials->cmd_buffer, from->image, from_layout, to->buffer,
                             1, region);

    return copy_object_end(dev, essentials);
}

vk_error VulkanRender::transition_images(struct vk_device *dev, struct vk_render_essentials *essentials,
                                         struct vk_image *images, uint32_t image_count,
                                         VkImageLayout from, VkImageLayout to, VkImageAspectFlags aspect,
                                         const char *name) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    d.vkResetCommandBuffer(essentials->cmd_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    res = d.vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Couldn't begin recording a command buffer to transition the %s image\n", name);
        return retval;
    }

    VkImageMemoryBarrier image_barrier{};

    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.srcAccessMask = 0;
    image_barrier.dstAccessMask = 0;
    image_barrier.oldLayout = from;
    image_barrier.newLayout = to;
    image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.subresourceRange.aspectMask = aspect;
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    for (uint32_t i = 0; i < image_count; ++i) {
        image_barrier.image = images[i].image;
        d.vkCmdPipelineBarrier(essentials->cmd_buffer,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               0,
                               0, nullptr,
                               0, nullptr,
                               1, &image_barrier);
    }

    d.vkEndCommandBuffer(essentials->cmd_buffer);

    res = d.vkResetFences(dev->device, 1, &essentials->exec_fence);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return retval;
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &essentials->cmd_buffer;

    d.vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);
    res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
    vk_error_set_vkresult(&retval, res);

    return retval;
}

vk_error VulkanRender::create_staging_buffer(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                             struct vk_buffer *staging,
                                             uint8_t *contents, size_t size, const char *name) {
    vk_error retval = VK_ERROR_NONE;

    *staging = {};
    staging->size = static_cast<uint32_t>(size);
    staging->usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging->host_visible = true;

    retval = VulkanUtils::create_buffers(phy_dev, dev, staging, 1);
    if (!vk_error_is_success(&retval)) {
        vk_error_printf(&retval, "Failed to create staging %s buffer\n", name);
        return retval;
    }

    char staging_name[50];
    snprintf(staging_name, 50, "staging %s", name);
    retval = fill_buffer(dev, staging, contents, size, staging_name);

    return retval;
}

vk_error VulkanRender::transition_images_mipmaps(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                                 struct vk_render_essentials *essentials,
                                                 struct vk_image *image, VkImageAspectFlags aspect, const char *name) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    VkFormatProperties formatProperties;
    d.vkGetPhysicalDeviceFormatProperties(phy_dev->physical_device, image->format,
                                          &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        spdlog::error("texture image format does not support linear blitting! {} image", name);
        retval.error.type = VK_ERROR_ERRNO;
        return retval;
    }

    d.vkResetCommandBuffer(essentials->cmd_buffer, 0);
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    res = d.vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Couldn't begin recording a command buffer to transition the %s image\n", name);
        return retval;
    }

    auto mipWidth = static_cast<int32_t>(image->extent.width);
    auto mipHeight = static_cast<int32_t>(image->extent.height);
    auto mipLevels = static_cast<uint32_t>(
            (std::log(std::min(image->extent.width, image->extent.height)) / std::log(2)) + 1);

    VkImageMemoryBarrier image_barrier{};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.image = image->image;

    image_barrier.subresourceRange.aspectMask = aspect;
    image_barrier.subresourceRange.levelCount = 1;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = 1;

    for (uint32_t i = 1; i < mipLevels; i++) {
        image_barrier.subresourceRange.baseMipLevel = i - 1;
        image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        d.vkCmdPipelineBarrier(essentials->cmd_buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                               0, nullptr,
                               0, nullptr,
                               1, &image_barrier);

        VkImageBlit blit = {0};
        blit.srcOffsets[0] = (struct VkOffset3D) {.x=0, .y=0, .z=0};
        blit.srcOffsets[1] = (struct VkOffset3D) {.x=mipWidth, .y=mipHeight, .z=1};
        blit.srcSubresource.aspectMask = aspect;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = (struct VkOffset3D) {.x=0, .y=0, .z=0};
        blit.dstOffsets[1] = (struct VkOffset3D) {.x=(mipWidth > 1 ? mipWidth / 2 : 1), .y=(mipHeight > 1 ? mipHeight /
                                                                                                            2
                                                                                                          : 1), .z=1};
        blit.dstSubresource.aspectMask = aspect;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        d.vkCmdBlitImage(essentials->cmd_buffer,
                         image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1, &blit,
                         VK_FILTER_LINEAR);

        image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        d.vkCmdPipelineBarrier(essentials->cmd_buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                               0, nullptr,
                               0, nullptr,
                               1, &image_barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    image_barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    d.vkCmdPipelineBarrier(essentials->cmd_buffer,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0,
                           0, nullptr,
                           0, nullptr,
                           1, &image_barrier);

    d.vkEndCommandBuffer(essentials->cmd_buffer);

    res = d.vkResetFences(dev->device, 1, &essentials->exec_fence);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return retval;
    }

    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &essentials->cmd_buffer,
    };

    d.vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);
    res = d.vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
    vk_error_set_vkresult(&retval, res);

    return retval;
}

vk_error VulkanRender::update_texture(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                      struct vk_render_essentials *essentials,
                                      struct vk_image *image, VkImageLayout base_layout, uint8_t *contents,
                                      const char *name) {
    vk_error retval = VK_ERROR_NONE;

    struct vk_buffer staging{};
    retval = create_staging_buffer(phy_dev, dev, &staging, contents,
                                   image->extent.width * image->extent.height * 4, name);
    if (!vk_error_is_success(&retval))
        return retval;

    retval = transition_images(dev, essentials, image, 1, base_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_ASPECT_COLOR_BIT, name);
    if (!vk_error_is_success(&retval))
        return retval;

    VkBufferImageCopy image_copy = {
            .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
            },
            .imageExtent = {
                    .width = image->extent.width,
                    .height = image->extent.height,
                    .depth = 1,
            },
    };
    retval = copy_buffer_to_image(dev, essentials, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &staging,
                                  &image_copy, name);
    if (!vk_error_is_success(&retval))
        return retval;
    retval = transition_images(dev, essentials, image, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, base_layout,
                               VK_IMAGE_ASPECT_COLOR_BIT, name);
    if (!vk_error_is_success(&retval))
        return retval;

    VulkanUtils::free_buffers(dev, &staging, 1);
    return retval;
}

vk_error VulkanRender::init_texture(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                    struct vk_render_essentials *essentials,
                                    struct vk_image *image, VkImageLayout layout, uint8_t *contents, const char *name) {
    vk_error retval = VK_ERROR_NONE;

    struct vk_buffer staging{};
    retval = create_staging_buffer(phy_dev, dev, &staging, contents,
                                   image->extent.width * image->extent.height * 4, name);
    if (!vk_error_is_success(&retval))
        return retval;

    retval = transition_images(dev, essentials, image, 1, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, name);
    if (!vk_error_is_success(&retval))
        return retval;

    VkBufferImageCopy image_copy = {
            .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
            },
            .imageExtent = {
                    .width = image->extent.width,
                    .height = image->extent.height,
                    .depth = 1,
            },
    };

    retval = copy_buffer_to_image(dev, essentials, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &staging,
                                  &image_copy, name);
    if (!vk_error_is_success(&retval))
        return retval;

    if (image->mipmaps) {
        retval = transition_images_mipmaps(phy_dev, dev, essentials, image, VK_IMAGE_ASPECT_COLOR_BIT, name);
        if (retval.error.type == VK_ERROR_ERRNO)
            retval = transition_images(dev, essentials, image, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       layout, VK_IMAGE_ASPECT_COLOR_BIT, name);
    } else
        retval = transition_images(dev, essentials, image, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout,
                                   VK_IMAGE_ASPECT_COLOR_BIT, name);

    VulkanUtils::free_buffers(dev, &staging, 1);
    return retval;
}

vk_error VulkanRender::init_buffer(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                   struct vk_render_essentials *essentials,
                                   struct vk_buffer *buffer, void *contents, const char *name) {
    vk_error retval = VK_ERROR_NONE;

    struct vk_buffer staging{};
    retval = create_staging_buffer(phy_dev, dev, &staging, (uint8_t *) contents, buffer->size, name);
    if (!vk_error_is_success(&retval))
        return retval;

    retval = copy_buffer(dev, essentials, buffer, &staging, buffer->size, name);
    VulkanUtils::free_buffers(dev, &staging, 1);

    return retval;
}


int VulkanRender::finish(struct vk_render_essentials *essentials, struct vk_device *dev,
                         struct vk_swapchain *swapchain, VkImageLayout from_layout, uint32_t image_index,
                         VkSemaphore wait_sem, VkSemaphore signal_sem) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    VkImageMemoryBarrier image_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout = from_layout,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = essentials->images[image_index],
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            },
    };

    d.vkCmdPipelineBarrier(essentials->cmd_buffer,
                           VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                           0,
                           0, nullptr,
                           0, nullptr,
                           1, &image_barrier);

    d.vkEndCommandBuffer(essentials->cmd_buffer);

    res = d.vkResetFences(dev->device, 1, &essentials->exec_fence);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return res;
    }

    VkSemaphore wait_sems[2] = {essentials->sem_post_acquire, wait_sem};
    VkSemaphore signal_sems[2] = {essentials->sem_pre_submit, signal_sem};
    VkPipelineStageFlags wait_sem_stages[2] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = wait_sem ? UINT32_C(2) : UINT32_C(1),
            .pWaitSemaphores = wait_sems,
            .pWaitDstStageMask = wait_sem_stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &essentials->cmd_buffer,
            .signalSemaphoreCount = signal_sem ? UINT32_C(2) : UINT32_C(1),
            .pSignalSemaphores = signal_sems,
    };
    d.vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);

    VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &essentials->sem_pre_submit,
            .swapchainCount = 1,
            .pSwapchains = &swapchain->swapchain,
            .pImageIndices = &image_index,
    };
    res = d.vkQueuePresentKHR(essentials->present_queue, &present_info);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        return VK_ERROR_OUT_OF_DATE_KHR;
    } else if (res == VK_SUBOPTIMAL_KHR) {
        return 0;
    } else if (res == VK_ERROR_SURFACE_LOST_KHR) {
        return VK_ERROR_SURFACE_LOST_KHR;
    } else {
        vk_error_set_vkresult(&retval, res);
        if (res < 0) {
            vk_error_printf(&retval, "Failed to queue image for presentation\n");
            return -1;
        }
    }

    return 0;
}
