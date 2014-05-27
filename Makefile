MCU = atmega88
CFLAGS=-Os -Wall -Wextra

all: sanduhr.hex

sanduhr.elf: main.c i2c.c i2c.h uart.c uart.h timer.c timer.h gyro.c gyro.h accel.c accel.h common.h speaker.c speaker.h pwm.c pwm.h
	avr-gcc -std=gnu99 -mmcu=$(MCU) $(CFLAGS) -o $@ $^

sanduhr.hex: sanduhr.elf
	avr-objcopy -O ihex -R .eeprom $< $@

program: sanduhr.hex
	avrdude -p m88 -c avrispmkII -U flash:w:sanduhr.hex  -v -P usb
