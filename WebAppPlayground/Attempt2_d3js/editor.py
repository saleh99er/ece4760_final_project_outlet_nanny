import csv

row = ['2013-04-28T01:01:12:041','100.00']

with open('test.csv', 'a') as csvFile:
    writer = csv.writer(csvFile)
    writer.writerow(row)

csvFile.close()
