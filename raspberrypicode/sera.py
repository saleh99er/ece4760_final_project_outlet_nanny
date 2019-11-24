from serial import Serial
import csv

ser = Serial('COM11', baudrate = 9600, timeout = 0) # OPEN SERIAL PORT
print(ser.name) # PRINT PORT THAT WAS USED


while True:
	ser.write(b'r0')
	rcv = ser.read(1) # READ 1 BYTE
	value = (rcv)
	myString = str(value)
	
	if (myString == "b'1'"):
		ser.write(b'0')
	elif (myString == "b'0"):
		ser.write(b'1')

	print(myString)
	with open('D:\Samad\Desktop\sera.csv','a') as fd:
		if (myString != "b''"):
			fd.write(myString+"\n")
			fd.close()
		