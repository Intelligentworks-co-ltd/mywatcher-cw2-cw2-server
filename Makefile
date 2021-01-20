TARGET = cw2_server

CC = gcc
CXX = g++
CFLAGS = -std=c99 -ggdb -O2 -D`uname -s`
#CXXFLAGS = -std=c++98 -ggdb -O2 -I /usr/include/mariadb -D`uname -s`
CXXFLAGS = -std=c++17 -ggdb -O0 -I /usr/include/mariadb -D`uname -s`

all: $(TARGET)

include Objects


$(TARGET): buildinfo $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) -lpthread -lmariadb -ldl /usr/local/lib/libpcap.a /usr/local/lib/libpfring.a
#	strip $(TARGET)

buildinfo:
	ruby buildinfo.rb > buildinfo.tmp
	$(CXX) -c buildinfo.cpp

update:
	../makeObjects > Objects

clean:
	-rm buildinfo.tmp
	-rm *.o
	-cd common;rm *.o
