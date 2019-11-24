from serial import Serial
import csv
import time

salehsPath='C:\\Users\\Dell\\Documents\\Classes\\ECE4760\\ece4760-final-project\\raspberrypicode\\sera.csv'
samadsPath='D:\Samad\Desktop\sera.csv'
port = 'COM12'

ser = Serial(port, baudrate = 9600, timeout = 0) # OPEN SERIAL PORT
print(ser.name) # PRINT PORT THAT WAS USED
last_rcv_string = "b''"
rcv = 0

while True:

	ser.write(b'r 1')
	ser.write(b'\r')
	last_rcv_string = rcv

	rcv = ser.read(12) # READ 12 BYTES
	rcv_string = str(rcv)
	print(rcv_string)

	
	# if (myString == "b'1'"):
	# 	ser.write(b'0')
	# elif (myString == "b'0"):
	# 	ser.write(b'1')

	with open(salehsPath,'a') as fd:
		if (rcv_string != "b''"):
			print(rcv_string)
			fd.write(rcv_string+"\n")
			fd.close()
	time.sleep(1)
		