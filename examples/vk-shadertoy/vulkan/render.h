
#ifndef _LAUNCHER_VULKAN_RENDER_H_
#define _LAUNCHER_VULKAN_RENDER_H_

#include "common.h"

#include "utils.h"

#include "vk_error_print.h"


class VulkanUtils;

class VulkanRender {
public:
    VulkanRender();

    ~VulkanRender();

    static int get_essentials(struct vk_render_essentials *essentials, struct vk_physical_device *phy_dev,
                              struct vk_device *dev, struct vk_swapchain *swapchain);

    static void cleanup_essentials(struct vk_render_essentials *essentials, struct vk_device *dev);

    static VkResult start(struct vk_render_essentials *essentials, struct vk_device *dev,
                          struct vk_swapchain *swapchain, VkImageLayout to_layout, uint32_t *image_index);

    static vk_error
    fill_object(struct vk_device *dev, VkDeviceMemory to, void *from, size_t size, const char *object,
                const char *name);

    static vk_error fill_buffer(struct vk_device *dev, struct vk_buffer *to, void *from, size_t size, const char *name);

    static vk_error fill_image(struct vk_device *dev, struct vk_image *to, void *from, size_t size, const char *name);

    static vk_error
    copy_object_start(struct vk_device * /* dev */, struct vk_render_essentials *essentials, const char *object,
                      const char *name);

    static vk_error copy_object_end(struct vk_device *dev, struct vk_render_essentials *essentials);

    static vk_error copy_buffer(struct vk_device *dev, struct vk_render_essentials *essentials,
                                struct vk_buffer *to, struct vk_buffer *from, size_t size, const char *name);

    static vk_error copy_image(struct vk_device *dev, struct vk_render_essentials *essentials,
                               struct vk_image *to, VkImageLayout to_layout, struct vk_image *from,
                               VkImageLayout from_layout,
                               VkImageCopy *region, const char *name);

    static vk_error copy_buffer_to_image(struct vk_device *dev, struct vk_render_essentials *essentials,
                                         struct vk_image *to, VkImageLayout to_layout, struct vk_buffer *from,
                                         VkBufferImageCopy *region, const char *name);

    static vk_error copy_image_to_buffer(struct vk_device *dev, struct vk_render_essentials *essentials,
                                         struct vk_buffer *to, struct vk_image *from, VkImageLayout from_layout,
                                         VkBufferImageCopy *region, const char *name);

    static vk_error transition_images(struct vk_device *dev, struct vk_render_essentials *essentials,
                                      struct vk_image *images, uint32_t image_count,
                                      VkImageLayout from, VkImageLayout to, VkImageAspectFlags aspect,
                                      const char *name);

    static vk_error
    create_staging_buffer(struct vk_physical_device *phy_dev, struct vk_device *dev, struct vk_buffer *staging,
                          uint8_t *contents, size_t size, const char *name);

    static vk_error transition_images_mipmaps(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                              struct vk_render_essentials *essentials,
                                              struct vk_image *image, VkImageAspectFlags aspect, const char *name);

    static vk_error update_texture(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                   struct vk_render_essentials *essentials,
                                   struct vk_image *image, VkImageLayout base_layout, uint8_t *contents,
                                   const char *name);

    static vk_error init_texture(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                 struct vk_render_essentials *essentials,
                                 struct vk_image *image, VkImageLayout layout, uint8_t *contents, const char *name);

    static vk_error init_buffer(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                struct vk_render_essentials *essentials,
                                struct vk_buffer *buffer, void *contents, const char *name);


    static int finish(struct vk_render_essentials *essentials, struct vk_device *dev,
                      struct vk_swapchain *swapchain, VkImageLayout from_layout, uint32_t image_index,
                      VkSemaphore wait_sem, VkSemaphore signal_sem);
};

#endif //_LAUNCHER_VULKAN_RENDER_H_
