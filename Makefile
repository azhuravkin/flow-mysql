CC = gcc
CFLAGS = -Wall -O2
TARGET = flow-mysql
OBJECT = flow-mysql.o
LIBS = -L/usr/lib/mysql -lmysqlclient

all: $(TARGET)

$(TARGET): Makefile $(OBJECT)
	$(CC) $(CFLAGS) $(OBJECT) -o $(TARGET) $(LIBS)

$(OBJECT): Makefile flow-mysql.c
	$(CC) $(CFLAGS) -D'PROG_NAME="$(TARGET)"' -c flow-mysql.c -o $(OBJECT)

clean:
	rm -f $(TARGET) $(OBJECT)
