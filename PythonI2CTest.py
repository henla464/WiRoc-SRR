#!/usr/bin/python

import smbus

bus = smbus.SMBus(0)    # 0 = /dev/i2c-0 (port I2C0), 1 = /dev/i2c-1 (port I2C1)
addr = 0x20
# firmware version
firmwareversion = bus.read_byte_data(addr, 0x00)
print(firmwareversion)

# hardware features
hwf = bus.read_byte_data(addr, 0x01)
print(hwf)

# serialno
serial = bus.read_i2c_block_data(addr, 0x02, 4)
print(serial)

# error count
errcnt = bus.read_byte_data(addr, 0x03)
print(errcnt)


# status
status = bus.read_byte_data(addr, 0x04)
print(status)



# set dataindex to 32
bus.write_byte_data(addr, 0x05, 32)

# write serialno
bus.write_i2c_block_data(addr, 0x02, [0x6,0x7,0x8,0x9])

# punch length
plen = bus.read_byte_data(addr, 0x20)
print(plen)

# read punch
index = 0;
if plen > 0: # and (status & 0x01) > 0:
	while index < plen:
		bus.write_byte_data(addr, 0x05, index)
		remaining = plen - index
		noToRead = remaining
		if remaining > 31:
			noToRead = 31
		data = bus.read_i2c_block_data(addr, 0x40, noToRead)
		index += noToRead
		print(data)

# read error

# errormsg length
elen = bus.read_byte_data(addr, 0x27)
print(elen)

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


DEVICE_ADDRESS = 0x15      #7 bit address (will be left shifted to add the read write bit)
DEVICE_REG_MODE1 = 0x00
DEVICE_REG_LEDOUT0 = 0x1d

#Write a single register
bus.write_byte_data(DEVICE_ADDRESS, DEVICE_REG_MODE1, 0x80)

#Write an array of registers
ledout_values = [0xff, 0xff, 0xff, 0xff, 0xff, 0xff]
bus.write_i2c_block_data(DEVICE_ADDRESS, DEVICE_REG_LEDOUT0, ledout_values)



/////
from smbus2 import SMBus, i2c_msg

bus = SMBus(0)
read = i2c_msg.read(0x20, 30)
write = i2c_msg.write(0x20, 0x31)

