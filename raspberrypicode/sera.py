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



def setCurrentLimit(lim):
	assert type(lim) == float or type(lim) == int
	if(type(lim) == int):
		lim = float(lim)
	confirm = False
	while(not confirm):
		ser.write(b'l %f' % lim)
		ser.write(b'\r')
		rcv = ser.read(8)
		rcv_string = str(rcv)
		confirm = False
		if(rcv_string == b''):
			print(rcv_string)

def requestRelay(turnOn):
	assert type(turnOn) == bool
	confirm  = False
	while(not confirm):
		#print("not confirmed")
		if(turnOn):
			ser.write(b'r 1')
		else:
			ser.write(b'r 0')
		ser.write(b'\r')
		rcv = ser.read(24)
		rcv_string = str(rcv)
		print(rcv_string) #for debugging

		confirm = (turnOn and rcv_string == RELAY_ON_CONFIRM_MSG) or (not turnOn and rcv_string == RELAY_OFF_CONFIRM_MSG)
		#print(confirm) #for debugging
		time.sleep(0.1)

# TEST / Debugging functions

def debug_terminal():
	while True:
		rcv = b''
		rcv_string = ""
		no_response = True

		user_input = input(">>")
		
		if(user_input == "exit()"):
			break
		user_input = user_input + "\n"
		#print(user_input)
		user_input_bstr = b'%s' % user_input.encode('utf8')
		#print(user_input_bstr)
		while(no_response):
			ser.write(user_input_bstr)
			time.sleep(0.1)
			rcv = ser.read(16)
			rcv_string = str(rcv)
			if(rcv != b''):
				no_response = False
			else:
				no_response = True
		print(rcv_string)

def test_periodicRelayControl():
	toggle = False
	while True:
		requestRelay(toggle)
		print("done " + str(toggle))
		toggle = not toggle
		time.sleep(10)

def test_serialCommsToRaspPi():
	while True:
		ser.write("Hello World")
		rcv = ser.read(24)
		rcv_string = str(rcv)
		print(rcv_string)
		time.sleep(0.5)

# main
#test_periodicRelayControl()
debug_terminal()
		