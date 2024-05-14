
#pragma once

#include "vulkan/utils.h"
#include "vulkan/render.h"

class VulkanUtils;

class VulkanRender;

class ShaderToy : public VulkanUtils, public VulkanRender {
public:

    ShaderToy();

    ~ShaderToy();

    int init(int width, int height, struct wl_display *wl_display, struct wl_surface *wl_surface, uint32_t dev_index,
             bool use_gpu_idx, bool debug, bool reload_shaders,
             VkPresentModeKHR present_mode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR);

    void draw_frame(uint32_t time);

    struct app_os_window *get_app_os_window() { return &os_window_; }

    void toggle_pause() { os_window_.app_data.pause = !os_window_.app_data.pause; }

    void toggle_draw_debug() { os_window_.app_data.drawdebug = !os_window_.app_data.drawdebug; }

    void toggle_fps_lock() { os_window_.fps_lock = !os_window_.fps_lock; }

    void screen_shot() { screenshot_once_ = true; }

private:

    uint32_t resize_size_[2]{};

    bool main_image_srgb_ = false; // srgb surface fix

    // keyboard is texture that send from this data
    uint8_t keyboard_texture_[256 * 3 * 4]{}; // texture
    bool keyboard_draw_{};
    bool screenshot_once_{};

    // update to 2021 Shadertoy iMouse.w change https://www.shadertoy.com/view/llySRh (comments)
    bool last_iMousel_clicked_[2] = {};

    // do not edit, it just to see where keyboard texture used
    static constexpr uint32_t iKeyboard = UINT32_C(1);

    // to build-in compressed shaders into bin(exe) file
    // used OFFSCREEN_BUFFERS size, names of .hex files should be set manually(and edit yariv_shaders[]), this example using 4 buffers same as on shadertoy
    //#define YARIV_SHADER

    struct shaders_push_constants {
        float iMouse[4];
        float iDate[4];
        int iMouse_lr[2];
        float iResolution[2];
        int debugdraw; // look function check_hotkeys
        int pCustom; //custom data
        float iTime;
        float iTimeDelta;
        int iFrame;
    };

    enum {
        BUFFER_VERTICES = 0,
        BUFFER_INDICES = 1,
    };
    enum {
        SHADER_MAIN_VERTEX = 0,
        SHADER_MAIN_FRAGMENT = 1,
    };

    struct render_data {
        struct objects {
            struct vertex {
                float pos[3];
            } vertices[3];

            uint16_t indices[3];
        } objects;

        struct shaders_push_constants push_constants;

        struct vk_image images[IMAGE_TEXTURES + OFFSCREEN_BUFFERS + iKeyboard];
        struct vk_buffer buffers[2];
        struct vk_shader shaders[2 + OFFSCREEN_BUFFERS * 2];
        struct vk_graphics_buffers *main_gbuffers;
        struct vk_offscreen_buffers *buf_obuffers;

        VkRenderPass buf_render_pass[OFFSCREEN_BUFFERS];
        struct vk_layout buf_layout[OFFSCREEN_BUFFERS];
        struct vk_pipeline buf_pipeline[OFFSCREEN_BUFFERS];
        VkDescriptorSet buf_desc_set[OFFSCREEN_BUFFERS];

        VkRenderPass main_render_pass;
        struct vk_layout main_layout;
        struct vk_pipeline main_pipeline;
        VkDescriptorSet main_desc_set;

    } render_data_{};

    uint32_t dev_index_{};
    bool use_gpu_idx_{};

    VkInstance vk_{};
    struct vk_physical_device phy_dev_{};
    struct vk_device dev_{};
    struct vk_swapchain swapchain_{};
    struct app_os_window os_window_{};

    struct vk_render_essentials essentials_{};

    VkFence offscreen_fence_ = VK_NULL_HANDLE;
    VkQueue offscreen_queue_[OFFSCREEN_BUFFERS] = {VK_NULL_HANDLE};
    VkCommandBuffer offscreen_cmd_buffer_[OFFSCREEN_BUFFERS] = {VK_NULL_HANDLE};
    VkSemaphore wait_buf_sem_ = VK_NULL_HANDLE;
    VkSemaphore wait_main_sem_ = VK_NULL_HANDLE;
    bool first_submission_ = true;

    void update_key_map(int w, int h, bool val);

    void update_keypress();

    void check_hotkeys(struct app_os_window *os_window);

    vk_error allocate_render_data(struct vk_physical_device *phy_dev, struct vk_device *dev,
                                  struct vk_swapchain *swapchain, struct vk_render_essentials *essentials,
                                  struct render_data *render_data, bool reload_shaders);

    static void free_render_data(struct vk_device *dev, struct vk_render_essentials *essentials,
                                 struct render_data *render_data);

    static void exit_cleanup_render_loop(struct vk_device *dev, struct vk_render_essentials *essentials,
                                         struct render_data *render_data, VkSemaphore wait_buf_sem,
                                         VkSemaphore wait_main_sem, VkFence offscreen_fence);

    void
    render_loop_init(struct vk_physical_device *phy_dev, struct vk_device *dev, struct vk_swapchain *swapchain,
                     struct app_os_window *os_window);

    static void exit_cleanup(VkInstance vk, struct vk_device *dev, struct vk_swapchain *swapchain);

    bool on_window_resize(struct vk_physical_device *phy_dev, struct vk_device *dev,
                          struct vk_render_essentials *essentials, struct vk_swapchain *swapchain,
                          struct render_data *render_data, struct app_os_window *os_window);

    static void update_params(struct app_data_struct *app_data, bool fps_lock);

    void set_push_constants(struct app_os_window *os_window);

    void update_push_constants_window_size(struct app_os_window *os_window);

    void update_push_constants_local_size(float width, float height);

    static bool render_loop_buf(struct vk_physical_device * /* phy_dev */, struct vk_device *dev,
                                struct vk_render_essentials *essentials, struct render_data *render_data,
                                VkCommandBuffer cmd_buffer, int render_index, int buffer_index,
                                struct app_data_struct * /* app_data */);

    bool
    render_loop_draw(struct vk_physical_device *phy_dev, struct vk_device *dev, struct vk_swapchain *swapchain,
                     struct app_os_window *os_window);

    bool update_iKeyboard_texture(struct vk_physical_device * /* phy_dev */, struct vk_device * /* dev */,
                                  struct vk_render_essentials * /* essentials */,
                                  struct render_data * /* render_data */);

    static vk_error
    transition_images_screenshot_swapchain_begin(struct vk_device *dev, struct vk_render_essentials *essentials,
                                                 struct vk_image *srcImage, struct vk_image *dstImage);

    static vk_error
    transition_images_screenshot_swapchain_end(struct vk_device *dev, struct vk_render_essentials *essentials,
                                               struct vk_image *srcImage, struct vk_image *dstImage);

// RGBA BMP from https://en.wikipedia.org/wiki/BMP_file_format
    static inline unsigned char ev(int32_t v);

    static void write_bmp(uint32_t w, uint32_t h, const uint8_t *rgba);

    static vk_error
    make_screenshot(struct vk_physical_device *phy_dev, struct vk_device *dev, struct vk_swapchain *swapchain,
                    struct vk_render_essentials *essentials, struct render_data *render_data,
                    uint32_t image_index);
};