#import matplotlib.pyplot as plt
import numpy as np
import datetime as dt


with open('plotter_test.csv','r') as file:
  print("Printing file")
  line_no = 0
  for line in file:
    line_no += 1
    if(line_no == 1): #skip column header 
      continue
    print("New Line")

    try:
      date_and_time = line.split(',')[0]
      print(date_and_time)
      time = dt.datetime.strptime(date_and_time,"%Y-%m-%d %H:%M:%S")
      print("Time is...")
      print(time)

    except:
      print("Exception occurred")


#plt.plot(,y, label='Loaded from file!')
#plt.xlabel('Date')
#plt.ylabel('Value')
#plt.title('Woah')
#plt.legend()
#plt.show()
