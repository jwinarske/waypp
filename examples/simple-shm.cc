#include <csignal>

#include <cxxopts.hpp>

#include "window/xdg_toplevel.h"

#include "logging.h"

typedef struct {
    int width;
    int height;
    bool fullscreen;
    int maximized;
    bool fullscreen_ratio;
    bool tearing;
} CONFIGURATION_T;

static volatile bool running = true;


/**
 * @brief Signal handler function to handle signals.
 *
 * This function is a signal handler for handling signals. It sets the value of keep_running
 * to false, which will stop the program from running. The function does not take any input
 * parameters.
 *
 * @param signal The signal number. This parameter is not used by the function.
 *
 * @return void
 */
void handle_signal(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}

static void
paint_pixels(void *image, int padding, int width, int height, uint32_t time) {
    const int halfh = padding + (height - padding * 2) / 2;
    const int halfw = padding + (width - padding * 2) / 2;
    int ir, or_;
    auto *pixel = static_cast<uint32_t *>(image);
    int y;

    /* squared radii thresholds */
    or_ = (halfw < halfh ? halfw : halfh) - 8;
    ir = or_ - 32;
    or_ *= or_;
    ir *= ir;

    pixel += padding * width;
    for (y = padding; y < height - padding; y++) {
        int x;
        int y2 = (y - halfh) * (y - halfh);

        pixel += padding;
        for (x = padding; x < width - padding; x++) {
            uint32_t v;

            /* squared distance from center */
            int r2 = (x - halfw) * (x - halfw) + y2;

            if (r2 < ir)
                v = (static_cast<uint32_t>(r2 / 32) + time / 64) * 0x0080401;
            else if (r2 < or_)
                v = (static_cast<uint32_t>(y) + time / 32) * 0x0080401;
            else
                v = (static_cast<uint32_t>(x) + time / 16) * 0x0080401;
            v &= 0x00ffffff;

            /* cross if compositor uses X from XRGB as alpha */
            if (abs(x - y) > 6 && abs(x + y - height) > 6)
                v |= 0xff000000;

            *pixel++ = v;
        }

        pixel += padding;
    }
}

void draw_frame(void *data, const uint32_t time) {
    auto window = static_cast<Window *>(data);

    window->prune_old_released_buffers();

    auto buffer = window->next_buffer();
    if (!buffer) {
        spdlog::error("Failed to acquire a buffer");
        abort();
    }

    paint_pixels(buffer->get_shm_data(), 20, window->get_width(), window->get_height(), time);

    wl_surface_attach(window->get_surface(), buffer->get_wl_buffer(), 0, 0);
    wl_surface_damage(window->get_surface(), 20, 20, window->get_width() - 40, window->get_height() - 40);

    buffer->set_busy();
}

int main(int argc, char **argv) {

    auto gLogging = std::make_unique<Logging>();

    std::signal(SIGINT, handle_signal);

    cxxopts::Options options("simple-shm", "Weston simple-shm example");
    options.add_options()
            ("w,width", "Set width", cxxopts::value<int>()->default_value("250"))
            ("h,height", "Set height", cxxopts::value<int>()->default_value("250"))
            ("f,fullscreen", "Run in fullscreen mode")
            ("m,maximized", "Run in maximized mode")
            ("r,fullscreen-ratio", "Use fixed width/height ratio when run in fullscreen mode")
            ("t,tearing", "Enable tearing via the tearing_control protocol");
    auto result = options.parse(argc, argv);

    CONFIGURATION_T config = {
            .width = result["width"].as<int>(),
            .height = result["height"].as<int>(),
            .fullscreen = result["fullscreen"].as<bool>(),
            .maximized = result["maximized"].as<bool>() ? 1 : 0,
            .fullscreen_ratio = result["fullscreen-ratio"].as<bool>(),
            .tearing = result["tearing"].as<bool>(),
    };

    XdgWindowManager wm = XdgWindowManager();
    spdlog::info("XDG Window Manager Version: {}", wm.get_version());
    auto top_level = wm.CreateTopLevel("simple-shm",
                                       config.width,
                                       config.height,
                                       2,
                                       WL_SHM_FORMAT_XRGB8888,
                                       config.fullscreen,
                                       config.maximized,
                                       config.fullscreen_ratio,
                                       config.tearing,
                                       draw_frame
    );
    spdlog::info("XDG Window Version: {}", top_level->get_version());

    /// paint padding
    top_level->set_surface_damage(0, 0, config.width, config.height);
    top_level->update_buffer_geometry();
    top_level->start_frame_callbacks();

    while (running && top_level->is_valid() && wm.display_dispatch() != -1) {}

    top_level->stop_frame_callbacks();

    return EXIT_SUCCESS;
}