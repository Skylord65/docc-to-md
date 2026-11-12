#ifndef COMPAT_WIN32_UNISTD_H
#define COMPAT_WIN32_UNISTD_H

#include <io.h>        /* _read, _write, _close, _access, _isatty, _unlink */
#include <direct.h>    /* _getcwd, _mkdir, _chdir, _rmdir */
#include <process.h>   /* _getpid */
#include <stdlib.h>
#include <sys/types.h>
#include <BaseTsd.h>

typedef SSIZE_T ssize_t;

#ifndef PATH_MAX
  #define PATH_MAX _MAX_PATH
#endif

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define W_OK 2
#define R_OK 4

static inline ssize_t uwu_write(int fd, const void *buf, size_t n) {
  return (ssize_t)_write(fd, buf, (unsigned int)n);
}
static inline ssize_t uwu_read(int fd, void *buf, size_t n) {
  return (ssize_t)_read(fd, buf, (unsigned int)n);
}
#define write uwu_write
#define read  uwu_read
#define close   _close
#define access  _access
#define isatty  _isatty
#define unlink  _unlink
#define getcwd  _getcwd
#define chdir   _chdir
#define rmdir   _rmdir
#define getpid  _getpid

#endif