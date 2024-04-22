#include "./vm.h"

int main(int argc, const char *argv[]) {
  VM::vm machine = *new VM::vm();
  if (argc < 2) {
    /* show usage string */
    printf("lc3 [image-file1] ...\n");
    exit(2);
  }

  for (int j = 1; j < argc; ++j) {
    if (!machine.read_image(argv[j])) {
      printf("failed to load image: %s\n", argv[j]);
      exit(1);
    }
  }

  machine.execute();

  return 0;
}