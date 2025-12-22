CC = gcc
CFLAGS = -Wall -I. -I./system
LIBS = -lncurses

# Находим все си-файлы в текущей папке и в подпапке system
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
