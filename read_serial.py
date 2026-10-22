import serial
import time
import sys

try:
    s = serial.Serial()
    s.port = 'COM3'
    s.baudrate = 115200
    s.dtr = False
    s.rts = False
    s.timeout = 0.2
    s.open()
    start = time.time()
    while time.time() - start < 10.0:
        if s.in_waiting > 0:
            data = s.read(s.in_waiting)
            sys.stdout.write(data.decode('utf-8', errors='ignore'))
            sys.stdout.flush()
        time.sleep(0.05)
    s.close()
except Exception as e:
    print(f"Error: {e}")
