# This script runs an automated voltage sweep while collecting ADC DBGLN() output
# from an ExpressLRS RX using devAnalogVbat. The receiver should be compiled with:
#   -DDEBUG_VBAT_ADC -DDEBUG_LOG -DDEBUG_CRSF_NO_OUTPUT -DRCVR_UART_BAUD=460800
# (lots of UART errors at the default 420000)
# If firmware is workign correctly `cat /dev/ttyUSB0` will show values such as
#   $ADC,925
#   $ADC,924
#   $ADC,926
#   ...

# Uses the KoradSerial library for power supply control, just dump this in the library directory
# https://github.com/starforgelabs/py-korad-serial
from koradserial import KoradSerial
import time
import serial
import argparse

class MedianAvg:
    def __init__(self):
        self.reset()
    def reset(self):
        self.vals = []
        self.min = 0
        self.max = 0
        self.mean = 0
    def add(self, val):
        self.vals.append(val)
    def calc(self):
        if len(self.vals) < 3:
            self.reset()
            return
        self.vals.sort()
        self.min = self.vals[0]
        self.max = self.vals[-1]
        self.mean = 0
        for i in range(1, len(self.vals)-1):
            self.mean += self.vals[i]
        self.mean = self.mean / (len(self.vals)-2)
    def __len__(self):
        return len(self.vals)
    def __str__(self):
        return f'n={len(self.vals)} mean={self.mean} min={self.min} max={self.max}'

def flushSerial(ser):
    while True:
        bVal = ser.read_until()
        if len(bVal) == 0:
            return

def doVoltageSample(ser, voltage):
    flushSerial(ser)
    adcMedian = MedianAvg()
    while True:
        sLine = ser.read_until().decode('ascii')
        if sLine.startswith('$ADC,') and sLine.endswith('\r\n'):
            try:
                iADC = int(sLine[5:-2])
                #print(iADC)
                adcMedian.add(iADC)
                if len(adcMedian) >= 10:
                    adcMedian.calc()
                    print(f'{voltage:0.1f},{adcMedian.mean:0.4f},{adcMedian.min},{adcMedian.max}')
                    adcMedian.reset()
                    return
            except ValueError:
                pass

def runSweep(args):
    with serial.Serial(args.rport, baudrate=args.rbaud, timeout=0.010) as rcvr:
        with KoradSerial(args.kport) as pwr:
            try:
                #print("Model: ", pwr.model)
                pwrch = pwr.channels[0]
                pwrch.voltage = 5.00
                pwr.output.on()

                print('voltage,mean,min,max')
                for v in range(args.start, args.end+args.step, args.step):
                    voltage = v / 100.0
                    pwrch.voltage = voltage
                    time.sleep(0.2)
                    doVoltageSample(rcvr, voltage)
            finally:
                pwr.output.off()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Run a voltage sweep using a Korad power supply and recevie ADC data from an ELRS receiver")
    parser.add_argument("-b", "--rbaud", type=int, default=460800,
        help="Receiver UART baud")
    parser.add_argument("-e", "--end", type=int, default=100,
        help="Ending voltage (centidegrees 500 = 5.00V)")
    parser.add_argument("-k", "--kport", type=str, default='/dev/ttyACM0',
        help="Port used for Korad power supply UART")
    parser.add_argument("-r", "--rport", type=str, default='/dev/ttyUSB0',
        help="Port used for receiver UART")
    parser.add_argument("-s", "--start", type=int, default=3100,
        help="Starting voltage (centidegrees 500 = 5.00V)")
    parser.add_argument("-t", "--step", type=int, default=-10,
        help="Voltage step (centidegrees -10 = -0.10V)")

    args = parser.parse_args()

    runSweep(args)

