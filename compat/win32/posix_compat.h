#ifndef POSIX_COMPAT_H
#define POSIX_COMPAT_H
#include <fcntl.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdlib.h>
#include <limits.h>

#define realpath(path, resolved) _fullpath((resolved), (path), PATH_MAX)

#define mkdir(path, mode) _mkdir(path)

#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IXUSR _S_IEXEC
#define S_IRGRP 0   
#define S_IROTH 0
#define S_IWGRP 0
#define S_IWOTH 0

#ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
  #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#endif