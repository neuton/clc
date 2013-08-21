NAME = clcmpl
HEADER = cl_error_codes.h
CC = C:/MinGW/bin/gcc
CFLAGS = -O -I"C:/Program Files (x86)/AMD APP/include" -L"C:/Program Files (x86)/AMD APP/lib/x86" -lOpenCL

ifeq ($(OS), Windows_NT)
	EXEC = $(NAME).exe
	RM = del
else
	EXEC = $(NAME)
	RM = rm
endif

$(EXEC): $(NAME).c $(HEADER)
	$(CC) $< -o $@ $(CFLAGS)

clean:
	$(RM) $(EXEC)
