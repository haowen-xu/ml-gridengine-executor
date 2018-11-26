//
// Created by 许昊文 on 2018/11/26.
//

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: Count N\n");
    exit(1);
  }
  char *endc;
  long n = strtol(argv[1], &endc, 10);
  for (long i=0; i<n; ++i) {
    printf("%ld\n", i);
  }
}
