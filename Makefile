CC = gcc
CFLAGS = -Wall -O2 -I/usr/include/cjson
LIBS = -lcurl -lcjson

all: weather_data

weather_data: weather_data.o
	$(CC) weather_data.o -o weather_data $(LIBS)

weather_data.o: weather_data.c weather_data.h
	$(CC) $(CFLAGS) -c weather_data.c

run: all
	./weather_data

clean:
	rm -f *.o weather_data raw_data.txt processed_data.txt

