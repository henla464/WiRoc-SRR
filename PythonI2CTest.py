#!/usr/bin/python3

import smbus

bus = smbus.SMBus(0)    # 0 = /dev/i2c-0 (port I2C0), 1 = /dev/i2c-1 (port I2C1)
addr = 0x20


## REGISTER ADDRESSES ##
FIRMWAREVERSIONREGADDR = 0x00   	# Version of the firware
HARDWAREFEATURESREGADDR = 0x01  	# Hardware features available: bit 0: RED Channel, bit 1: BLUE Channel, bit 2: Send errors on UART
SERIALNOREGADDR = 0x02  		# Serialno of the dongle
ERRORCOUNTREGADDR = 0x03  		# Serialno of the dongle
STATUSREGADDR = 0x04	  		# Indicates what messages there is to fetch. bit 7: Error message, bit 0: Punch message
SETDATAINDEXREGADDR = 0x05  		# Index to the block data a register

## Length registers
PUNCHLENGTHREGADDR = 0x20	  	# Lenght of next punch message
ERRORLENGTHREGADDR = 0x27		# Lenght of error message

## Message data registers
PUNCHREGADDR = 0x40	  		# Read punch message
ERRORMSGREGADDR = 0x47			# Read last error message

# firmware version
firmwareversion = bus.read_byte_data(addr, FIRMWAREVERSIONREGADDR)
print("Firmware version: %s" %(firmwareversion))

# hardware features. bit 1 = RED Channel exists, bit 2 = BLUE channel exists
hwf = bus.read_byte_data(addr, HARDWAREFEATURESREGADDR)
print("Hardware features: %s " %(hwf))

# serialno
serial = bus.read_i2c_block_data(addr, SERIALNOREGADDR, 4)
print("Serial number: %s " %(serial))

# error count
errcnt = bus.read_byte_data(addr, ERRORCOUNTREGADDR)
print("Error count: %s " %(errcnt))

# set dataindex to 32
dataIndex = 32
print("Set data index to: %s" %(dataIndex))
bus.write_byte_data(addr, SETDATAINDEXREGADDR, dataIndex)
print("Done")

# write serialno
serialNo = [0x6,0x7,0x8,0x9]
print("Set serialno to %s" %(serialNo))
bus.write_i2c_block_data(addr, SERIALNOREGADDR, serialNo)
print("Done")

# punch length
plen = bus.read_byte_data(addr, PUNCHLENGTHREGADDR)
print("Number of bytes in the next punch to fetch: %s" %(plen))

# status. Status byte: bit 7 set = error message exists, bit 1 set = punch message exists
status = bus.read_byte_data(addr, STATUSREGADDR)
print("Status: %s " %(status))


# Example for checking if punch exists and fetch.
# This checks the status register if a punch exists
if status & 0x01:
	print("Punch exist to fetch")
	index = 0
	if plen > 0:
		while index < plen:
			print("Set data index to: %s" %(index))
			bus.write_byte_data(addr, 0x05, index)
			print("Done");
			remaining = plen - index
			noToRead = remaining
			if remaining > 31:
				noToRead = 31
			data = bus.read_i2c_block_data(addr, 0x40, noToRead)
			index += noToRead
			print("Punch data:")
			print(data)
else:
	print("No punch to fetch")


# punch length
plen = bus.read_byte_data(addr, PUNCHLENGTHREGADDR)
print("Number of bytes in the next punch to fetch: %s" %(plen))


# Example for checking if punch exists and fetch.
# But actually it is not required. Punch length will be 0 if there is no message to fetch. So can just check that instead.
# Note that there is also an IRQ line that can be checked. It will be high if there are any punches to fetch.
index = 0
if plen > 0:
	print("Punch exist to fetch")
	while index < plen:
		print("Set data index to: %s" %(index))
		bus.write_byte_data(addr, 0x05, index)
		print("Done");
		remaining = plen - index
		noToRead = remaining
		if remaining > 31:
			noToRead = 31
		data = bus.read_i2c_block_data(addr, 0x40, noToRead)
		index += noToRead
		print("Punch data:")
		print(data)
else:
	print("No punch to fetch")


# errormsg length
elen = bus.read_byte_data(addr, 0x27)
print("Number of bytes in the next error message to fetch: %s " %(elen))

if status & 0x80:
	print("Error message exist to fetch")
	print("Error message data:")
	index = 0;
	if elen > 0: # and (status & 0x80) > 0:
		while index < elen:
			bus.write_byte_data(addr, 0x05, index)
			remaining = elen - index
			noToRead = remaining
			if remaining > 5:
				noToRead = 5
			data = bus.read_i2c_block_data(addr, 0x47, noToRead)
			index += noToRead
			print(data)
else:
	print("No error message to fetch")

