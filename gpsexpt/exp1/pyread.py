import serial
from datetime import datetime

outF = open("mesh.txt", "w")
with serial.Serial('/dev/ttyUSB0', 9600, timeout=1) as ser:
    while True:
        dateTimeObj = datetime.now()
        timeObj = dateTimeObj.time()
        timeStr = timeObj.strftime("%H:%M:%S.%f")
        line=ser.readline().strip('\r').strip('\n')
        print(line)
        outF.write(timeStr+'&'+line+'\n')
        outF.flush()
    