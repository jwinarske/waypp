/*
 * Copyright © 2024 Joel Winarske
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "anonymous_file.h"

#include <filesystem>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <waypp/waypp.h>
#include <cerrno>
#include <cstdlib>

#include "logging/logging.h"

#if !defined(HAVE_MKOSTEMP)

int os_fd_set_cloexec(int fd) {
  int flags;

  if (fd == -1)
    return -1;

  flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    return -1;

  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
    return -1;

  return 0;
}

static int set_cloexec_or_close(int fd) {
  if (os_fd_set_cloexec(fd) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

#endif

static int create_tmpfile_cloexec(char* tmpname) {
  int fd;

#if HAVE_MKOSTEMP
  fd = mkostemp(tmpname, O_CLOEXEC);
  if (fd >= 0)
    unlink(tmpname);
#else
  fd = mkstemp(tmpname);
  if (fd >= 0) {
    fd = set_cloexec_or_close(fd);
    unlink(tmpname);
  }
#endif

  return fd;
}

/*
 * Create a new, unique, anonymous file of the given size, and
 * return the file descriptor for it. The file descriptor is set
 * CLOEXEC. The file is immediately suitable for mmap()'ing
 * the given size at offset zero.
 *
 * The file should not have a permanent backing store like a disk,
 * but may have if XDG_RUNTIME_DIR is not properly implemented in OS.
 *
 * The file name is deleted from the file system.
 *
 * The file is suitable for buffer sharing between processes by
 * transmitting the file descriptor over Unix sockets using the
 * SCM_RIGHTS methods.
 *
 * If the C library implements posix_fallocate(), it is used to
 * guarantee that disk space is available for the file at the
 * given size. If disk space is insufficient, errno is set to ENOSPC.
 * If posix_fallocate() is not supported, program may receive
 * SIGBUS on accessing mmap()'ed file contents instead.
 *
 * If the C library implements memfd_create(), it is used to create the
 * file purely in memory, without any backing file name on the file
 * system, and then sealing off the possibility of shrinking it.  This
 * can then be checked before accessing mmap()'ed file contents, to
 * make sure SIGBUS can't happen.  It also avoids requiring
 * XDG_RUNTIME_DIR.
 */
int AnonymousFile::create(off_t size) {
  int fd;
  int ret;

#if HAVE_MEMFD_CREATE
  fd = memfd_create("waypp-shared", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd >= 0) {
    /* We can add this seal before calling posix_fallocate(), as
     * the file is currently zero-sized anyway.
     *
     * There is also no need to check for the return value, we
     * couldn't do anything with it anyway.
     */
    fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
  } else
#endif
  {
    const auto xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) {
      errno = ENOENT;
      return -1;
    }

    std::filesystem::path tmp_path(xdg_runtime_dir);
    tmp_path /= "/waypp-shared-XXXXXX";
    DLOG_DEBUG("Creating tmp file: {}", tmp_path.c_str());
    std::string path(tmp_path);
    fd = create_tmpfile_cloexec(path.data());
    if (fd < 0)
      return -1;
  }

#if HAVE_POSIX_FALLOCATE
  do {
    ret = posix_fallocate(fd, 0, size);
  } while (ret == EINTR);
  if (ret != 0) {
    close(fd);
    errno = ret;
    return -1;
  }
#else
  do {
    ret = ftruncate(fd, size);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    close(fd);
    return -1;
  }
#endif

  return fd;
}
