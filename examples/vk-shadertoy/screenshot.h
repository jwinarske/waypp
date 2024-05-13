#include "vulkan/render.h"

class VulkanRender;

vk_error
ShaderToy::transition_images_screenshot_swapchain_begin(struct vk_device *dev, struct vk_render_essentials *essentials,
                                                        struct vk_image *srcImage, struct vk_image *dstImage) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    vkResetCommandBuffer(essentials->cmd_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    res = vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Couldn't begin recording a command buffer to screenshot image\n");
        return retval;
    }

    VkImageMemoryBarrier image_barrier_dstImage{};
    image_barrier_dstImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier_dstImage.srcAccessMask = 0;
    image_barrier_dstImage.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier_dstImage.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier_dstImage.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier_dstImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_dstImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_dstImage.image = dstImage->image;
    image_barrier_dstImage.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier_dstImage.subresourceRange.baseMipLevel = 0;
    image_barrier_dstImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    image_barrier_dstImage.subresourceRange.baseArrayLayer = 0;
    image_barrier_dstImage.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(essentials->cmd_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &image_barrier_dstImage);

    VkImageMemoryBarrier image_barrier_srcImage{};
    image_barrier_srcImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier_srcImage.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    image_barrier_srcImage.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_barrier_srcImage.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_barrier_srcImage.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_barrier_srcImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_srcImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_srcImage.image = srcImage->image;
    image_barrier_srcImage.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier_srcImage.subresourceRange.baseMipLevel = 0;
    image_barrier_srcImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    image_barrier_srcImage.subresourceRange.baseArrayLayer = 0;
    image_barrier_srcImage.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(essentials->cmd_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &image_barrier_srcImage);

    vkEndCommandBuffer(essentials->cmd_buffer);

    res = vkResetFences(dev->device, 1, &essentials->exec_fence);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return retval;
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &essentials->cmd_buffer;

    vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);
    res = vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return retval;
    }

    return retval;
}

vk_error
ShaderToy::transition_images_screenshot_swapchain_end(struct vk_device *dev, struct vk_render_essentials *essentials,
                                                      struct vk_image *srcImage, struct vk_image *dstImage) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    vkResetCommandBuffer(essentials->cmd_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    res = vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Couldn't begin recording a command buffer to screenshot image\n");
        return retval;
    }

    VkImageMemoryBarrier image_barrier_dstImage{};
    image_barrier_dstImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier_dstImage.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier_dstImage.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    image_barrier_dstImage.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier_dstImage.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_barrier_dstImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_dstImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_dstImage.image = dstImage->image;
    image_barrier_dstImage.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier_dstImage.subresourceRange.baseMipLevel = 0;
    image_barrier_dstImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    image_barrier_dstImage.subresourceRange.baseArrayLayer = 0;
    image_barrier_dstImage.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(essentials->cmd_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &image_barrier_dstImage);

    VkImageMemoryBarrier image_barrier_srcImage{};
    image_barrier_srcImage.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier_srcImage.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_barrier_srcImage.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    image_barrier_srcImage.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_barrier_srcImage.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_barrier_srcImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_srcImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_srcImage.image = srcImage->image;
    image_barrier_srcImage.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier_srcImage.subresourceRange.baseMipLevel = 0;
    image_barrier_srcImage.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    image_barrier_srcImage.subresourceRange.baseArrayLayer = 0;
    image_barrier_srcImage.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(essentials->cmd_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &image_barrier_srcImage);

    vkEndCommandBuffer(essentials->cmd_buffer);

    res = vkResetFences(dev->device, 1, &essentials->exec_fence);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return retval;
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &essentials->cmd_buffer;

    vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);
    res = vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
    vk_error_set_vkresult(&retval, res);
    if (res) {
        vk_error_printf(&retval, "Failed to reset fence\n");
        return retval;
    }

    return retval;
}

// RGBA BMP from https://en.wikipedia.org/wiki/BMP_file_format
unsigned char ev(int32_t v) {
    static uint32_t counter = 0;
    return (unsigned char) ((v) >> ((8 * (counter++)) % 32));
}

void write_bmp(uint32_t w, uint32_t h, const uint8_t *rgba) {

    static int scr_id = 0;

    if (!rgba) {
        return;
    }

    auto img = (unsigned char *) malloc(4 * w * h);
    memset(img, 0, 4 * w * h);

    for (unsigned int x = 0; x < w; x++) {
        for (unsigned int y = 0; y < h; y++) {
            img[(x + y * w) * 4 + 3] = rgba[(x + (h - 1 - y) * w) * 4 + 0];
            img[(x + y * w) * 4 + 2] = rgba[(x + (h - 1 - y) * w) * 4 + 1];
            img[(x + y * w) * 4 + 1] = rgba[(x + (h - 1 - y) * w) * 4 + 2];
            img[(x + y * w) * 4 + 0] = rgba[(x + (h - 1 - y) * w) * 4 + 3];
        }
    }

    auto filesize = static_cast<int32_t>(108 + 14 + 4 * w * h);
    unsigned char bmp_file_header[14] = {'B', 'M', ev(filesize), ev(filesize), ev(filesize), ev(filesize), 0, 0, 0, 0,
                                         108 + 14, 0, 0, 0};

    unsigned char bmp_info_header[108] = {108, 0, 0, 0,
                                          ev(static_cast<int32_t>(w)), ev(static_cast<int32_t>(w)), ev(
                    static_cast<int32_t>(w)), ev(static_cast<int32_t>(w)), ev(-((int32_t) h)), ev(-((int32_t) h)),
                                          ev(-((int32_t) h)), ev(-((int32_t) h)), 1, 0, 32, 0, 3, 0, 0, 0, ev(
                    static_cast<int32_t>(w * h * 4)),
                                          ev(static_cast<int32_t>(w * h * 4)), ev(static_cast<int32_t>(w * h * 4)), ev(
                    static_cast<int32_t>(w * h * 4)),
                                          ev(0x0b13), ev(0x0b13), ev(0x0b13), ev(0x0b13), ev(0x0b13), ev(0x0b13),
                                          ev(0x0b13), ev(0x0b13),
                                          0, 0, 0, 0, 0, 0, 0, 0,
                                          0, 0, 0, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0, 0,
                                          ev(0x57696E20), ev(0x57696E20), ev(0x57696E20), ev(0x57696E20),
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                          0,
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    std::ostringstream ss;
    ss << "screenshot_" << scr_id++ << ".bmp";
    auto f = fopen(ss.str().c_str(), "wb");
    fwrite(bmp_file_header, 1, 14, f);
    fwrite(bmp_info_header, 1, 108, f);
    for (int i = 0; i < h; i++) {
        fwrite(img + (w * (h - static_cast<uint32_t>(i) - 1) * 4), 4, w, f);
    }

    free(img);
    fclose(f);
}

vk_error
ShaderToy::make_screenshot(struct vk_physical_device *phy_dev, struct vk_device *dev, struct vk_swapchain *swapchain,
                           struct vk_render_essentials *essentials, struct render_data *render_data,
                           uint32_t image_index) {
    vk_error retval = VK_ERROR_NONE;
    VkResult res;

    if (!essentials->first_render) {
        res = vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
        vk_error_set_vkresult(&retval, res);
        if (res) {
            vk_error_printf(&retval, "Wait for fence failed\n");
            return retval;
        }
    }

    uint32_t support_format_list[4] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB,
                                       VK_FORMAT_R8G8B8A8_UNORM};
    bool supported = false;
    for (int i = 0; (i < 4) && (!supported); i++) {
        if (swapchain->surface_format.format == support_format_list[i])supported = true;
    }
    supported &= swapchain->surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (!supported) {
        vk_error_printf(&retval, "Can not save screenshot, surface has unique format or not supported transfer %lu\n",
                        (unsigned long) swapchain->surface_format.format);
        return retval;
    }

    struct vk_image srcImage{};
    srcImage.image = essentials->images[image_index];

    struct vk_image dstImage{};
    dstImage.format = VK_FORMAT_R8G8B8A8_UNORM; //VK_FORMAT_R8G8B8A8_SRGB
    dstImage.extent = render_data->main_gbuffers[image_index].surface_size;
    dstImage.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    dstImage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    dstImage.make_view = false;
    dstImage.host_visible = true;
    dstImage.anisotropyEnable = true;
    dstImage.repeat_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT; //VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER //VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    dstImage.mipmaps = false;
    dstImage.linear = true;

    retval = VulkanUtils::create_images(phy_dev, dev, &dstImage, 1);
    if (!vk_error_is_success(&retval)) {
        vk_error_printf(&retval, "Failed to create dstImage for screenshot\n");
        return retval;
    }

    retval = transition_images_screenshot_swapchain_begin(dev, essentials, &srcImage, &dstImage);
    if (!vk_error_is_success(&retval)) {
        return retval;
    }

    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = dstImage.extent.width;
    imageCopyRegion.extent.height = dstImage.extent.height;
    imageCopyRegion.extent.depth = 1;

    retval = VulkanRender::copy_image(dev, essentials, &dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &srcImage,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &imageCopyRegion, "screenshot");
    if (!vk_error_is_success(&retval)) {
        vk_error_printf(&retval, "Failed to copy image for screenshot\n");
        return retval;
    }

    retval = transition_images_screenshot_swapchain_end(dev, essentials, &srcImage, &dstImage);
    if (!vk_error_is_success(&retval)) {
        return retval;
    }

    VkImageSubresource subResource{};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(dev->device, dstImage.image, &subResource, &subResourceLayout);

    uint8_t *data;
    vkMapMemory(dev->device, dstImage.image_mem, 0, VK_WHOLE_SIZE, 0, (void **) &data);
    data += subResourceLayout.offset;

    int color_order[3] = {0, 1, 2};

    if (swapchain->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB ||
        swapchain->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
        color_order[0] = 2;
        color_order[1] = 1;
        color_order[2] = 0;
    }

    uint8_t *data_rgba;
    data_rgba = (uint8_t *) malloc(4 * dstImage.extent.width * dstImage.extent.height);
    for (uint32_t y = 0; y < dstImage.extent.height; y++) {
        auto row = (uint8_t *) data;
        for (uint32_t x = 0; x < dstImage.extent.width; x++) {
            data_rgba[(x + y * dstImage.extent.width) * 4 + 0] = (uint8_t) row[x * 4 +
                                                                               static_cast<uint32_t>(color_order[0])];
            data_rgba[(x + y * dstImage.extent.width) * 4 + 1] = (uint8_t) row[x * 4 +
                                                                               static_cast<uint32_t>(color_order[1])];
            data_rgba[(x + y * dstImage.extent.width) * 4 + 2] = (uint8_t) row[x * 4 +
                                                                               static_cast<uint32_t>(color_order[2])];
            data_rgba[(x + y * dstImage.extent.width) * 4 + 3] = (uint8_t) row[x * 4 + 3];
        }
        data += subResourceLayout.rowPitch;
    }

    write_bmp(dstImage.extent.width, dstImage.extent.height, data_rgba);

    printf("screenshot done\n");

    free(data_rgba);
    vkUnmapMemory(dev->device, dstImage.image_mem);
    VulkanUtils::free_images(dev, &dstImage, 1);

    return retval;
}
