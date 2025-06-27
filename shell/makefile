
CC = g++
TARGET = net_test
INCLUDES += -I./

CompileFlags += -g -c -Wall

SRCS = main.c
OBJS = $(SRCS:.c=.o)

# 公用
LIBS += -L/usr/local/lib/

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(CompileFlags) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)

all:$(TARGET)

clean:
	rm -rf $(TARGET) *.o *~
