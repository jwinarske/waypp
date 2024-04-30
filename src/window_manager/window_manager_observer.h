
#pragma once

class WindowManagerObserver {
public:
    virtual ~WindowManagerObserver() = default;

    virtual void notify_task() = 0;
};
