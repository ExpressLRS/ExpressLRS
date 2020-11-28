import os
import platform
import sys
platform.platform()

platform_name = platform.system().lower()

python_exe = '"%s"' % sys.argv[1]
python_file = '"%s"' % sys.argv[2]
command = '"%s %s %s"' % (python_exe, python_file, sys.argv[3])

print("\n")
print("Going to use this python: " + python_exe)



if "windows" in platform_name:
    sys.stdout.write("Windows based operating system detected\n")
elif "linux" in platform_name:
    sys.stdout.write("Linux operating system detected\n")
elif "darwin" in platform_name:
    sys.stdout.write("Darwin operating system detected\n")
else:
    print("Unknown operating system...\n")
    raise OSError
    sys.stdout.flush()
    raise SystemExit

sys.stdout.flush()
try:
    os.system(command)
    sys.stdout.flush()
except:
    sys.stdout.write("no syscall provided, exiting...\n")
    sys.stdout.flush()
    raise SystemExit
