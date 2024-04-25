#pragma once

#include <sys/types.h>

struct AnonymousFile {
    static int create(off_t size);
};