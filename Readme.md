gcc main.c -o mytool \
    -I/usr/lib/llvm-14/include \
    -L/usr/lib/llvm-14/lib \
    -lclang
