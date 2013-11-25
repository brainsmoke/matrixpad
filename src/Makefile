# ======================================================================
# Template Makefile
#
# Copyright 2006-2010 Dick Streefland
#
# This is free software, licensed under the terms of the GNU General
# Public License as published by the Free Software Foundation.
# ======================================================================

TARGET_ARCH	= -mmcu=atmega328p
OBJECTS		= main.o
FLASH_CMD	= avrdude -v -c usbtiny -pm328p -U flash:w:main.hex 
FUSES_CMD	= avrdude -v -c usbtiny -pm328p -U lfuse:w:0xe2:m -U hfuse:w:0xd9:m -U efuse:w:0xff:m
STACK		= 32
FLASH		= 262144
SRAM		= 2048

include common.mk
