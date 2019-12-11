from serial import Serial
import csv
import time

salehsPathWindows='C:\\Users\\Dell\\Documents\\Classes\\ECE4760\\ece4760-final-project\\raspberrypicode\\sera.csv'
salehsPathUbuntu="/home/saleh99er/Documents/schoolStuff/ECE4760/4760_Final/sera.csv"

samadsPath='D:\Samad\Desktop\sera.csv'
port = 'COM12'
ubuntuPort = "/dev/ttyUSB0"


ser = Serial(port, baudrate = 9600, timeout = 0) # OPEN SERIAL PORT
print(ser.name) # PRINT PORT THAT WAS USED
last_rcv_string = "b''"
rcv = 0

END_MSG = '\\x00'
NEW_LINE = '\\n'
RELAY_OFF_CONFIRM_MSG = "b'" + NEW_LINE + 'r0 done' + END_MSG + "'"
RELAY_ON_CONFIRM_MSG = "b'" + NEW_LINE + 'r1 done' + END_MSG + "'"

toggle = True

def setCurrentLimit(lim):
	assert type(lim) == float or type(lim) == int
	if(type(lim) == int):
		lim = float(lim)
	confirm = False
	while(not confirm):
		ser.write(b'l %f' % lim)
		ser.write(b'\r')
		rcv = ser.read(16)
		rcv_string = str(rcv)
		confirm = False
		if(rcv_string == b''):
			print(rcv_string)
	
	



def requestRelay(turnOn):
	assert type(turnOn) == bool
	confirm  = False
	while(not confirm):
		print("not confirmed")
		if(turnOn):
			ser.write(b'r 1')
		else:
			ser.write(b'r 0')
		ser.write(b'\r')
		rcv = ser.read(16)
		rcv_string = str(rcv)
		#print(rcv_string) #for debugging

		confirm = (turnOn and rcv_string == RELAY_ON_CONFIRM_MSG) or (not turnOn and rcv_string == RELAY_OFF_CONFIRM_MSG)
		#print(confirm) #for debugging
		time.sleep(0.5)

# TEST / Debugging functions

def debug_terminal():
	while True:
		time.sleep(0.5)
		user_input = input(">>")
		print(user_input)
		user_input_bstr = b'%s' % user_input.encode('utf8')
		print(user_input_bstr)
		ser.write(user_input_bstr)
		rcv = ser.read(16)
		rcv_string = str(rcv)
		print(rcv_string)

def test_periodicRelayControl():
	toggle = False
	while True:
		requestRelay(toggle)
		print("done " + str(toggle))
		toggle = not toggle
		time.sleep(20)

def test_serialCommsToRaspPi():
	while True:
		ser.write("Hello World")
		rcv = ser.read(12)
		rcv_string = str(rcv)
		print(rcv_string)
		time.sleep(0.5)

#setCurrentLimit(5.0)
test_periodicRelayControl()
#debug_terminal()

# while True:

# 	ser.write(b'r 0')
# 	ser.write(b'\r')
# 	last_rcv_string = rcv

# 	rcv = ser.read(12) # READ 12 BYTES
# 	rcv_string = str(rcv)

	
# 	# if (myString == "b'1'"):
# 	# 	ser.write(b'0')
# 	# elif (myString == "b'0"):
# 	# 	ser.write(b'1')

# 	with open(salehsPath,'a') as fd:
# 		if (rcv_string != "b''"):
# 			print(rcv_string)
# 			fd.write(rcv_string+"\n")
# 			fd.close()
# 	time.sleep(1)
		