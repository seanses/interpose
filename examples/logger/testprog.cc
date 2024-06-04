#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  printf("Hello\n");

  int *p = (int *)malloc(sizeof(int));
  printf("p is %p\n", p);
  free(p);

  FILE *fp = fopen("hello.txt", "r");
  char buf[100];
  size_t nbytes = fread(buf, 1, 100, fp);
  printf("read %zu bytes\n", nbytes);
  printf("%s\n", buf);
  fclose(fp);

  int fd = open("hello.txt", O_RDONLY, DEFFILEMODE);
  size_t nbyte = read(fd, buf, 100);
  printf("read %zu bytes\n", nbytes);
  printf("%s\n", buf);
  close(fd);

  exit(0);
}
