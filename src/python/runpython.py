import os
import platform
import sys
platform.platform()

platform_name = ""

platform_name = platform.system().lower()

print("\n")

if "windows" in platform_name:
    sys.stdout.write("Windows based operating system detected\n")
    sys.stdout.flush()
    try:
        os.system("py -3 " + (sys.argv[1]))
        sys.stdout.flush()
    except:
        sys.stdout.write("no syscall provided, exiting...\n")
        sys.stdout.flush()
        raise SystemExit
    
elif "linux" in platform_name:
    sys.stdout.write("Linux operating system detected\n")
    sys.stdout.flush()
    try:
        os.system("python3 " + (sys.argv[1]))
        sys.stdout.flush()
    except:
        sys.stdout.write("no syscall provided, exiting...\n")
        sys.stdout.flush()
        raise SystemExit
    
elif "os x" in platform_name:
    sys.stdout.write("Mac operating system detected\n")
    sys.stdout.flush()
    try:
        os.system("python3 " + (sys.argv[1]))
        sys.stdout.flush()
    except:
        sys.stdout.write("no syscall provided, exiting...\n")
        sys.stdout.flush()
        raise SystemExit
    
else:
    print("Unknown operating system...\n")
    raise OSError
    sys.stdout.flush()
    raise SystemExit
