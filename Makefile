CC=avr-gcc
CXX=avr-g++
LD=avr-gcc

FLAGS=-mmcu=atmega32 -DF_CPU=20000000ULL

CFLAGS=$(FLAGS) -Os -Wall -Wextra
LFLAGS=$(FLAGS)

all: main.elf

flash: main.hex
	avrdude -P /dev/ttyUSB0 -p m32 -c stk500v2 -e -U flash:w:$<

clean: $(wildcard *.o)
	rm $^

main.elf: main.o
	$(LD) $(LFLAGS) -o $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.hex: %.elf
	avr-objcopy -O ihex -R .eeprom $< $@

.PHONY: flash clean

