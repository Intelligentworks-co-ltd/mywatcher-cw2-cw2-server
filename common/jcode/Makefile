


SRCS = \
	jcode.cpp \
	unicode.c
HEADS = \
	jcode.h \
	jstring.h \
	memoryreader.h \
	memorywriter.h \
	streamreader.h \
	streamwriter.h \
	stringwriter.h \
	widememorywriter.h \
	widestringwriter.h
OBJS = \
	jcode.o \
	unicode.o


LD	= g++

CC = gcc
CXX = g++

# CFLAGS	= -O2 -Wall
CFLAGS	= -ggdb -Wall -DDBG -DDBG_LOG_OUTPUT -I/usr/include/mysql
# CFLAGS	= -ggdb -O2 -Wall

all: $(OBJS)

.c.o:
	$(CC) -c $(CFLAGS) $(HEADDIR) $< -o $@

.cpp.o:
	$(CXX) -c $(CFLAGS) $(HEADDIR) $< -o $@
	
clean:
	rm -f $(OBJS)

