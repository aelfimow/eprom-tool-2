CFLAGS  = -mmcu=atmega8515
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -gdwarf-2
CFLAGS += -DF_CPU=9600000UL
CFLAGS += -Os
CFLAGS += -funsigned-char
CFLAGS += -funsigned-bitfields
CFLAGS += -fpack-struct
CFLAGS += -fshort-enums
CFLAGS += -Wstrict-prototypes
CFLAGS += -std=gnu99
CFLAGS += -I.

all: compile link hex listing symbols sizecheck
	@echo No errors

compile:
	avr-gcc -c main.c -o main.o ${CFLAGS}
	avr-gcc -c UsartDriver.c -o UsartDriver.o ${CFLAGS}

link:
	avr-gcc main.o UsartDriver.o --output main.elf -Wl,-Map=main.map,--cref -lm

hex:
	avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature main.elf main.hex

listing:
	avr-objdump -h -S -z main.elf > main.lss

symbols:
	avr-nm -n main.elf > main.sym

sizecheck:
	avr-size --mcu=atmega8515 --format=avr main.elf
