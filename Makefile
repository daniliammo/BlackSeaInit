CC = gcc
CFLAGS = -Wall -Wextra -Os -s -fdata-sections -ffunction-sections -Wl,--gc-sections -flto -fno-stack-protector
TARGET = Собранное/init
SRC = основа.c процессы.c динамический_список.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -D -m 755 $(TARGET) /sbin/$(TARGET)
