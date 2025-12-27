CC = gcc
CFLAGS = -Wall -I. -I./system
LIBS = -lncurses

SRC = main.c ui.c system/system.c
OBJ = $(SRC:.c=.o)
TARGET = lainux-installer

all: $(TARGET)

$(TARGET): $(OBJ)
 $(CC) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.c
 $(CC) $(CFLAGS) -c $< -o $@

clean:
 rm -f *.o system/*.o $(TARGET)
