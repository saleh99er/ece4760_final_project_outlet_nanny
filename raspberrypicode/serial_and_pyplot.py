import matplotlib.pyplot as plt
import numpy as np
import datetime as dt
import sera

tt = []
value = []

def append_current_reading():
    

def read_csv():
    with open('plotter_test.csv','r') as file:
    #print("Printing file")
        line_no = 0
        for line in file:
            line_no += 1
            if(line_no == 1): #skip column header 
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
    #print(tt)
    #print(value)

def draw_irms_plot():
    print("# of values " + str(len(value)))

    now = dt.datetime.now()
    last_updated_str = 'Last updated ' + now.strftime("%m %d %H:%M:%S")
    plt.plot(tt,value, label=last_updated_str)
    #plt.plot(time,value)
    plt.xlabel('Date')
    plt.ylabel('Current')
    plt.title('Avg RMS Current consumed over time')
    plt.gcf().autofmt_xdate() #beautify x-labels
    plt.legend()
    plt.show()
    #plt.savefig("avg_irms_over_time", edgecolor='b')

test_current_serial_reading()
read_csv()
draw_irms_plot()
