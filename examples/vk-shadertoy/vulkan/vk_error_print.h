
// Danil, 2021+ Vulkan shader launcher, self https://github.com/danilw/vulkan-shadertoy-launcher
// The MIT License

#ifndef EXAMPLES_VK_SHADERTOY_VULKAN_VK_ERROR_PRINT_H_
#define EXAMPLES_VK_SHADERTOY_VULKAN_VK_ERROR_PRINT_H_

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cerrno>

#define VULKAN_HPP_NO_EXCEPTIONS 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

#ifdef _GNUC
#define ATTR_UNUSED __attribute__((format(printf, 3, 4)))
#else
#define ATTR_UNUSED
#endif

enum vk_error_type {
    VK_ERROR_SUCCESS = 0,
    VK_ERROR_VKRESULT,
    VK_ERROR_VKRESULT_WARNING,
    VK_ERROR_ERRNO,
};

typedef struct vk_error_data {
    enum vk_error_type type;
    union {
        VkResult vkresult;
        int err_no;
    };
    const char *file;
    unsigned int line;
} vk_error_data;

typedef struct vk_error {
    struct vk_error_data error;
    struct vk_error_data sub_error; /*
                         * Used in cases where error is e.g. "VK_INCOMPLETE", and it is due to
                         * another error.
                         */
} vk_error;

#define VK_ERROR_NONE (struct vk_error){ .error = { .type = VK_ERROR_SUCCESS,}, .sub_error = { .type = VK_ERROR_SUCCESS,}, }

#define vk_error_set_vkresult(es, e)     vk_error_data_set_vkresult(&(es)->error,     (e), __FILE__, __LINE__)
#define vk_error_set_errno(es, e)        vk_error_data_set_errno   (&(es)->error,     (e), __FILE__, __LINE__)
#define vk_error_sub_set_vkresult(es, e) vk_error_data_set_vkresult(&(es)->sub_error, (e), __FILE__, __LINE__)
#define vk_error_sub_merge(es, os)       vk_error_data_merge(&(es)->sub_error, &(os)->error)

void vk_error_data_set_vkresult(struct vk_error_data *error, VkResult vkresult, const char *file, unsigned int line);

void vk_error_data_set_errno(struct vk_error_data *error, int err_no, const char *file, unsigned int line);

bool vk_error_data_merge(struct vk_error_data *error, struct vk_error_data *other);

bool vk_error_is_success(struct vk_error *error);

bool vk_error_is_warning(struct vk_error *error);

bool vk_error_is_error(struct vk_error *error);

#define vk_error_printf(es, ...) vk_error_fprintf(stdout, (es), __VA_ARGS__)

void vk_error_fprintf(FILE *fout, struct vk_error *error, const char *fmt, ...) ATTR_UNUSED;


const char *vk_VkPhysicalDeviceType_string(VkPhysicalDeviceType type);


#endif // EXAMPLES_VK_SHADERTOY_VULKAN_VK_ERROR_PRINT_H_
