MCU = atmega88

all: sanduhr.hex

sanduhr.elf: main.c i2c.c i2c.h uart.c
	avr-gcc -std=gnu99 -mmcu=$(MCU) -Os -o $@ $^

sanduhr.hex: sanduhr.elf
	avr-objcopy -O ihex -R .eeprom $< $@

program: sanduhr.hex
	avrdude -p m88 -c avrispmkII -U flash:w:sanduhr.hex  -v -P usb
