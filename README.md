clc
==============
Simple utility for compiling OpenCL kernels into binaries.

Usage examples:
--------------

`$ clc -h` -- display short usage help

`$ clc -l` -- list available platforms and devices

`$ clc kernel.cl -p AMD -d GPU -o kernel.bin -Werror`

`$ clc -o kernel.bin kernel.cl -p 1 -d 0 -cl-opt-disable -cl-single-precision-constant`
