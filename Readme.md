# Compile
gcc main.c -o mytool \
    -I/usr/lib/llvm-14/include \
    -L/usr/lib/llvm-14/lib \
    -lclang

# Test 
./mytool test.c

# Result
```
mydev@mydev-virtual-machine:~/Optimize/Checker$ ./mytool test.c

=== Analyzing: test.c ===
[WARNING] Potentially unsafe function 'gets' at test.c:11:5
[WARNING] Format string bug with 'printf' at test.c:24:5
```