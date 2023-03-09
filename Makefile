EXEC = clc
ARCH = 64
CC = gcc
CCFLAGS = -O
LDFLAGS =

ifdef AMDAPPSDKROOT
	CCFLAGS := $(CCFLAGS) -I"$(AMDAPPSDKROOT)/include"
	ifeq ($(ARCH), 64)
		LDFLAGS := $(LDFLAGS) -L"$(AMDAPPSDKROOT)/lib/x86_64" -lOpenCL
	else
		LDFLAGS := $(LDFLAGS) -L"$(AMDAPPSDKROOT)/lib/x86" -lOpenCL
	endif
endif

ifeq ($(OS), Windows_NT)
	SHELL = cmd
	EXEC := $(EXEC).exe
	RM = del
else
	SHELL = /bin/sh
	RM = rm -f
	ifeq ($(shell uname -s), Darwin)
		ifndef AMDAPPSDKROOT
			CC = clang
			LDFLAGS := $(LDFLAGS) -framework OpenCL
		endif
	else
		ifndef AMDAPPSDKROOT
			LDFLAGS := $(LDFLAGS) -L"/usr/lib/x86_64-linux-gnu" -lOpenCL
		endif
	endif
endif

$(EXEC): clc.o cl_error.o
	$(CC) $^ -o $@ $(LDFLAGS)

clc.o: clc.c cl_error.h Makefile
	$(CC) -c $< -o $@ $(CCFLAGS)

cl_error.o: cl_error.c cl_error.h Makefile
	$(CC) -c $< -o $@ $(CCFLAGS)

clean:
	$(RM) $(EXEC) clc.o cl_error.o

.PHONY: clean
