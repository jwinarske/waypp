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

// Danil, 2021+ Vulkan shader launcher, self
// https://github.com/danilw/vulkan-shadertoy-launcher The MIT License

#ifndef EXAMPLES_VK_SHADERTOY_TEXTURES_H_
#define EXAMPLES_VK_SHADERTOY_TEXTURES_H_

#include "vulkan/render.h"

#include "logging/logging.h"

class VulkanRender;

static vk_error init_texture_mem(struct vk_physical_device* phy_dev,
                                 struct vk_device* dev,
                                 struct vk_render_essentials* essentials,
                                 struct vk_image* image,
                                 uint8_t* texture,
                                 const int width,
                                 const int height,
                                 const char* name,
                                 const bool mipmaps,
                                 const bool linear) {
  auto retval = VK_ERROR_NONE;
  constexpr VkFormat img_format =
      VK_FORMAT_R8G8B8A8_UNORM;  // VK_FORMAT_R8G8B8A8_SRGB
  *image = (struct vk_image){
      .format = img_format,
      .extent = {.width = static_cast<uint32_t>(width),
                 .height = static_cast<uint32_t>(height)},
      .usage = static_cast<VkImageUsageFlagBits>(
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .make_view = true,
      .will_be_initialized = false,
      .host_visible = false,
      .multisample = false,
      .sharing_queues = nullptr,
      .sharing_queue_count = 0,
      .image = VK_NULL_HANDLE,
      .image_mem = VK_NULL_HANDLE,
      .view = VK_NULL_HANDLE,
      .sampler = VK_NULL_HANDLE,
      .anisotropyEnable = true,
      .repeat_mode =
          VK_SAMPLER_ADDRESS_MODE_REPEAT,  // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                                           // //VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
      .mipmaps = mipmaps,
      .linear = linear || mipmaps,
  };

  retval = VulkanUtils::create_images(phy_dev, dev, image, 1);
  if (!vk_error_is_success(&retval)) {
    retval.error.type = VK_ERROR_ERRNO;
    vk_error_printf(&retval, "Failed to create texture images\n");
    return retval;
  }
  retval = VulkanRender::init_texture(phy_dev, dev, essentials, image,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      texture, name);
  return retval;
}

#ifdef USE_stb_image

static vk_error init_texture_file(struct vk_physical_device* phy_dev,
                                  struct vk_device* dev,
                                  struct vk_render_essentials* essentials,
                                  struct vk_image* image,
                                  const char* name,
                                  bool mipmaps) {
  vk_error retval = VK_ERROR_NONE;
  int width, height, channels;
  stbi_set_flip_vertically_on_load(true);  // flip image Y
  uint8_t* generated_texture =
      stbi_load(name, &width, &height, &channels, STBI_rgb_alpha);
  if (generated_texture == nullptr) {
    retval.error.type = VK_ERROR_ERRNO;
    spdlog::error("Error in loading image {}", name);
    return retval;
  }

  retval = init_texture_mem(phy_dev, dev, essentials, image, generated_texture,
                            width, height, name, mipmaps, true);
  stbi_image_free(generated_texture);
  return retval;
}

#endif

static vk_error texture_empty(struct vk_physical_device* phy_dev,
                              struct vk_device* dev,
                              struct vk_render_essentials* essentials,
                              struct vk_image* image,
                              int width,
                              int height) {
  vk_error retval = VK_ERROR_NONE;
  size_t texture_size =
      static_cast<unsigned long>(width * height * 4) * sizeof(uint8_t);
  auto* generated_texture = static_cast<uint8_t*>(malloc(texture_size));
  if (generated_texture == nullptr) {
    retval.error.type = VK_ERROR_ERRNO;
    spdlog::error("Error in allocating memory");
    return retval;
  }
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const size_t pixel =
          (static_cast<unsigned int>(i) * static_cast<unsigned int>(width) +
           static_cast<unsigned int>(j)) *
          4 * sizeof(uint8_t);
      generated_texture[pixel + 0] = 0x00;
      generated_texture[pixel + 1] = 0x00;
      generated_texture[pixel + 2] = 0x00;
      generated_texture[pixel + 3] = 0x00;
    }
  }
  retval = init_texture_mem(phy_dev, dev, essentials, image, generated_texture,
                            width, height, "empty", false, false);
  free(generated_texture);
  return retval;
}

#endif  // EXAMPLES_VK_SHADERTOY_TEXTURES_H_