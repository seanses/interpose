#if !defined(__APPLE__)
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <interpose.h>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#define malloc_usable_size malloc_size
#elif defined(__linux__)
#include <malloc.h>
#endif

#define START_COLOR "\033[01;33m"
#define END_COLOR "\033[0m"

size_t allocated_bytes = 0;
size_t freed_bytes = 0;

INTERPOSE_C(void *, malloc, (size_t sz), (sz))
{
  void *result = Real__malloc(sz);
  allocated_bytes += malloc_usable_size(result);
  return result;
}

INTERPOSE_C_VOID(free, (void *p), (p))
{
  freed_bytes += malloc_usable_size(p);
  Real__free(p);
}

INTERPOSE_C(void *, calloc, (size_t n, size_t sz), (n, sz))
{
  void *result = Real__calloc(n, sz);
  allocated_bytes += malloc_usable_size(result);
  return result;
}

INTERPOSE_C(void *, realloc, (void *p, size_t sz), (p, sz))
{
  size_t old_size = malloc_usable_size(p);
  void *result = Real__realloc(p, sz);
  size_t new_size = malloc_usable_size(p);

  if (result == p)
  {
    if (new_size > old_size)
      allocated_bytes += new_size - old_size;
    else
      freed_bytes = old_size - new_size;
  }
  else
  {
    freed_bytes += old_size;
    allocated_bytes += new_size;
  }

  return result;
}

INTERPOSE_C_VOID(exit, (int status), (status))
{
  fprintf(stderr, START_COLOR);
  fprintf(stderr, "\n\nProgram Allocation Stats\n");
  fprintf(stderr, "  allocated %zu bytes\n", allocated_bytes);
  fprintf(stderr, "      freed %zu bytes\n", freed_bytes);
  if (allocated_bytes >= freed_bytes)
  {
    fprintf(stderr, "     leaked %zu bytes\n", allocated_bytes - freed_bytes);
  }
  else
  {
    fprintf(stderr, "      freed %zu extra bytes\n", freed_bytes - allocated_bytes);
  }
  fprintf(stderr, END_COLOR);

  Real__exit(status);
}

int internal_open(const char *path, int oflag, mode_t mode);

// INTERPOSE_C(int, open, (const char *path, int oflag, mode_t mode), (path, oflag, mode))
// {
//   return internal_open(path, oflag, mode);
// }

INTERPOSE_C_LAMBDA(int, open, (const char *path, int oflag, ...), {va_list args; va_start(args, oflag); mode_t mode = va_arg(args, int); va_end(args); return open(path, oflag, mode); })
{
  va_list args;
  va_start(args, oflag);
  mode_t mode = va_arg(args, int);
  va_end(args);
  return internal_open(path, oflag, mode);
}

struct fd_desc
{
  int fd;
  size_t size;
  size_t pos;
};

static struct fd_desc xetfds[100];
static int xfidx = 0;

int internal_open(const char *path, int oflag, mode_t mode)
{
  fprintf(stderr, "internal open %s\n", path);
  int fd = Real__open(path, oflag, mode);

  if (strcmp(path, "/Users/di/tt/bsf13/.gitattributes") == 0)
  {
    xetfds[xfidx] = (struct fd_desc){
        fd, 1000, 0};
    ++xfidx;
  }
  else
  {
    fprintf(stderr, "didn't do special things\n");
  }
  return fd;
}

int __sflags(const char *mode, int *optr)
{
  int ret, m, o;

  switch (*mode++)
  {

  case 'r': /* open for reading */
    ret = __SRD;
    m = O_RDONLY;
    o = 0;
    break;

  case 'w': /* open for writing */
    ret = __SWR;
    m = O_WRONLY;
    o = O_CREAT | O_TRUNC;
    break;

  case 'a': /* open for appending */
    ret = __SWR;
    m = O_WRONLY;
    o = O_CREAT | O_APPEND;
    break;

  default: /* illegal mode */
    errno = EINVAL;
    return (0);
  }

  /* [rwa]\+ or [rwa]b\+ means read and write */
  if (*mode == 'b')
    mode++;
  if (*mode == '+')
  {
    ret = __SRW;
    m = O_RDWR;
    mode++;
    if (*mode == 'b')
      mode++;
  }
  if (*mode == 'x')
    o |= O_EXCL;
  *optr = m | o;
  return (ret);
}

FILE *internal_fopen(const char *__filename, const char *__mode);

INTERPOSE_C(FILE *, fopen, (const char *__filename, const char *__mode), (__filename, __mode))
{
  fprintf(stderr, "interpose fopen\n");
  return internal_fopen(__filename, __mode);
}

FILE *internal_fopen(const char *__filename, const char *__mode)
{
  int oflag;
  __sflags(__mode, &oflag);
  int fd = internal_open(__filename, oflag, DEFFILEMODE);
  return fdopen(fd, __mode);
}

ssize_t internal_read(int fd, void *buf, size_t nbyte);

INTERPOSE_C(ssize_t, read, (int fd, void *buf, size_t nbyte), (fd, buf, nbyte))
{
  fprintf(stderr, "interpose read\n");
  for (int i = 0; i < xfidx; i++)
  {
    if (xetfds[i].fd == fd)
    {
      return internal_read(fd, buf, nbyte);
    }
  }

  return Real__read(fd, buf, nbyte);
}

ssize_t internal_read(int fd, void *buf, size_t nbyte)
{
  fprintf(stderr, "internal read\n");
  char *src = "this is fun";
  strcpy(buf, src);
  return strlen(src) + 1;
}

size_t internal_fread(void *__ptr, size_t __size, size_t __nitems, FILE *__stream);

INTERPOSE_C(size_t, fread, (void *__ptr, size_t __size, size_t __nitems, FILE *__stream), (__ptr, __size, __nitems, __stream))
{
  fprintf(stderr, "interpose fread\n");
  for (int i = 0; i < xfidx; i++)
  {
    if (xetfds[i].fd == __stream->_file)
    {
      return internal_fread(__ptr, __size, __nitems, __stream);
    }
  }

  return Real__fread(__ptr, __size, __nitems, __stream);
}

size_t internal_fread(void *__ptr, size_t __size, size_t __nitems, FILE *__stream)
{
  fprintf(stderr, "internal fread\n");
  char *src = "this is fun";
  strcpy(__ptr, src);
  return strlen(src) + 1;
}