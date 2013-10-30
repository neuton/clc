NAME = clcmpl
ARCH = 64
CC = gcc

ifdef AMDAPPSDKROOT
	OCLINC = $(AMDAPPSDKROOT)/include
	ifeq ($(ARCH), 64)
		OCLLIB = $(AMDAPPSDKROOT)/lib/x86_64
	else
		OCLLIB = $(AMDAPPSDKROOT)/lib/x86
	endif
endif

CFLAGS = -O -I"$(OCLINC)" -L"$(OCLLIB)" -lOpenCL

ifeq ($(OS), Windows_NT)
	EXEC = $(NAME).exe
	RM = del
else
	EXEC = $(NAME)
	RM = rm
endif

HEADER = cl_error_codes.h

$(EXEC): $(NAME).c $(HEADER)
	$(CC) $< -o $@ $(CFLAGS)

clean:
	$(RM) $(EXEC)
