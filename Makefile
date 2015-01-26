EXEC = clc
ARCH = 64
CC = gcc
CCFLAGS = -O

ifdef AMDAPPSDKROOT
	OCLINC = $(AMDAPPSDKROOT)/include
	ifeq ($(ARCH), 64)
		OCLLIB = $(AMDAPPSDKROOT)/lib/x86_64
	else
		OCLLIB = $(AMDAPPSDKROOT)/lib/x86
	endif
endif

ifeq ($(OS), Windows_NT)
	EXEC := $(EXEC).exe
	RM = del
else
	RM = rm
endif

$(EXEC): clcmpl.o cl_error.o
	$(CC) $^ -o $@ -L"$(OCLLIB)" -lOpenCL $(CCFLAGS)

clcmpl.o: clcmpl.c cl_error.h Makefile
	$(CC) -c $< -o $@ -I"$(OCLINC)" $(CCFLAGS)

cl_error.o: cl_error.c cl_error.h Makefile
	$(CC) -c $< -o $@ -I"$(OCLINC)" $(CCFLAGS)

clean:
	$(RM) clcmpl.o cl_error.o

.PHONY: clean
