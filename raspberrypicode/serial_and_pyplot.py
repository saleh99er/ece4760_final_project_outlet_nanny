import matplotlib.pyplot as plt
import numpy as np
import datetime as dt
import sera
import csv
import time

tt = []
value = []
last_updated_str = 'Last updated ???'

filename = 'plotter_test.csv'

def append_current_reading():
    global last_updated_str

    currentReading = sera.getCurrent()
    now = dt.datetime.now()
    now_str = now.strftime("%Y-%m-%d %H:%M:%S")
    append_this_row = [now_str , str(currentReading)]

    with open(filename, 'a') as file:
        writer = csv.writer(file)
        writer.writerow(append_this_row)
    last_updated_str = 'Last updated ' + now.strftime("%m %d %H:%M:%S")
    file.close()


def read_csv():
    with open(filename,'r') as file:
    #print("Printing file")
        line_no = 0
        for line in file:
            line_no += 1
            if(line_no == 1): #skip column header 
                continue
            elif(line == "" or line == "\n"):
                line_no -= 1
                continue
            #print("New Line") #for debugging

            try:
                line_data = line.split(',')
                date_and_time = line_data[0]
                value_at_time = float(line_data[1])
                #print(date_and_time) #for debugging
                tt.append(dt.datetime.strptime(date_and_time,"%Y-%m-%d %H:%M:%S"))
                value.append(value_at_time)
                #print("Time is...") #for debugging
                #print(time)

            except:
                print("Exception occurred")
        file.close()
    #print(tt)
    #print(value)

def draw_irms_plot():
    global tt
    global value

    print("# of values " + str(len(value)))

    plt.plot(tt,value, label=last_updated_str)
    #plt.plot(time,value)
    plt.xlabel('Date')
    plt.ylabel('Current')
    plt.title('Avg RMS Current consumed over time')
    plt.gcf().autofmt_xdate() #beautify x-labels
    plt.legend()
    #plt.show()
    plt.savefig("avg_irms_over_time", edgecolor='b')

sera.requestRelay(True)
print("setting up the device...")
time.sleep(5)
#sera.debug_terminal()
print("done")
i = 0
while(1):
    i+= 1
    append_current_reading()
    read_csv()
    draw_irms_plot()
    if (i == 5):
	    input("")
    value = []
    tt = []
    #time.sleep(2)