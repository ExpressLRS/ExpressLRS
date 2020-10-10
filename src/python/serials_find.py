import serial
import sys, glob

def serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    result = []
    platform = sys.platform.lower()
    if platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif platform.startswith('linux') or platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        #ports = glob.glob('/dev/tty[A-Za-z]*')
        # List all ttyACM* and ttyUSB* ports only
        ports = glob.glob('/dev/ttyACM*')
        ports.extend(glob.glob('/dev/ttyUSB*'))
    elif platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.usbmodem*')
    else:
        raise EnvironmentError('Unsupported platform')

    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException) as error:
            if "permission denied" in str(error).lower():
                raise EnvironmentError("You don't have persmission to use serial port!")
            pass
    return result

def get_serial_port(debug=True):
    result = serial_ports()
    if debug:
        sys.stdout.write("Detected the following serial ports on this system: \n")
        sys.stdout.write(str(result)[1:-1])
        sys.stdout.write("\n")

    if len(result) == 0:
        raise EnvironmentError('No valid serial port detected or port already open')

    return result[0]