/*
 * Copyright 2024 Joel Winarske
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chrono>
#include <csignal>
#include <iostream>

#include <GLES2/gl2.h>

#include "window/window_egl.h"
#include "window_manager/window_manager.h"

static volatile bool keep_running = true;
constexpr int WINDOW_HEIGHT = 200;
constexpr int WINDOW_WIDTH = 200;

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
void handle_signal(int /* signal */) {
    keep_running = false;
}

/**
*
*/
static float hue_to_channel(const float *const hue, const int n) {
    const auto k = static_cast<const float>(fmod(n + ((*hue) * 3 / M_PI), 6));
    return 1 - MAX(0, MIN(MIN(k, 4 - k), 1));
}

/**
 * @brief Converts hue value to RGB.
 *
 * This function takes an array of hue values and calculates the corresponding RGB values using the hue_to_channel() function.
 * The resulting RGB values are stored in the given RGB array.
 *
 * @param hue Pointer to an array of hue values.
 * @param rgb Pointer to an array for storing the RGB values.
 */
static void hue_to_rgb(const float *const hue, float (*rgb)[3]) {
    (*rgb)[0] = hue_to_channel(hue, 5);
    (*rgb)[1] = hue_to_channel(hue, 3);
    (*rgb)[2] = hue_to_channel(hue, 1);
}

/**
 * Calculate the current hue value based on the current time.
 *
 * The hue value is calculated using the current system time, and a constant value for hue change.
 * The current time is obtained using std::chrono library, which provides utilities for time-related operations.
 * The hue change is a constant value calculated as (2 * M_PI) / 10, where M_PI is the mathematical constant pi.
 *
 * @return The calculated hue value as a float.
 */
static float calculate_hue() {
    static const auto hue_change = static_cast<const float>((2 * M_PI) / 10);
    auto tp = std::chrono::system_clock::now();
    auto tp_sec = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    std::chrono::nanoseconds ns = tp - tp_sec;
    auto t = static_cast<double>(tp_sec.time_since_epoch().count()) +
             static_cast<double>(ns.count()) * 1e-9;
    return static_cast<float>(fmod(t * hue_change, 2 * M_PI));
}

/**
 * @brief Updates the frame by drawing it.
 *
 * This function updates the frame by drawing it on the screen. It sets the OpenGL clear color based on the calculated hue,
 * clears the color buffer, swaps the buffers to display the updated frame, and clears the current rendering context.
 *
 * @param data A pointer to the WindowEgl object.
 * @param time The current time in milliseconds.
 */
void frame_update(void *data, uint32_t time) {
    std::cout << "draw_frame: " << time << std::endl;
    auto obj = static_cast<WindowEgl *>(data);
    (void) obj->make_current();

    auto hue = calculate_hue();
    float rgb[3] = {0, 0, 0};
    hue_to_rgb(&hue, &rgb);
    glClearColor(rgb[0], rgb[1], rgb[2], 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();

    (void) obj->swap_buffers();
    (void) obj->clear_current();
}

/**
 * @brief Main function for the program.
 *
 * This function initializes the window manager and creates a window with the specified dimensions and type.
 * It sets up a signal handler for SIGINT (Ctrl+C) to stop the program, and then enters a loop to handle window events.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of strings representing the command line arguments.
 * @return An integer representing the exit status of the program.
 */
int main(int /* argc */, char ** /* argv */) {
    std::signal(SIGINT, handle_signal);
    WindowManager wm(Window::ShellType::XDG);
    wm.create_window(WINDOW_WIDTH, WINDOW_HEIGHT,
                     WindowManager::WindowType::EGL, frame_update);

    while (keep_running && wm.poll_events(0) >= 0);
    return EXIT_SUCCESS;
}
