MCU = atmega88
CFLAGS=-Os -Wall -Wextra

all: sanduhr.hex

sanduhr.elf: main.c i2c.c i2c.h uart.c uart.h timer.c common.c timer.h gyro.c gyro.h accel.c accel.h common.h pwm.c pwm.h ui.c ui.h
	avr-gcc -std=gnu99 -mmcu=$(MCU) $(CFLAGS) -o $@ $^

sanduhr.hex: sanduhr.elf
	avr-objcopy -O ihex -R .eeprom $< $@

program: sanduhr.hex
	avrdude -p m88 -c avrispmkII -U flash:w:sanduhr.hex  -v -P usb

terminal:
	avrdude -p m88 -c avrispmkII -t -v -P usb

reset:
	avrdude -p m88 -c avrispmkII -n

