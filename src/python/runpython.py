import os
import platform
import sys
platform.platform()

platform_name = ""

platform_name = platform.system().lower()

print("\n")
print("Going to use this python: " +str(sys.argv[1]))



if "windows" in platform_name:
    sys.stdout.write("Windows based operating system detected\n")
    sys.stdout.flush()
    try:
        os.system((sys.argv[1]+" "+sys.argv[2]+" "+sys.argv[3]))
        sys.stdout.flush()
    except:
        sys.stdout.write("no syscall provided, exiting...\n")
        sys.stdout.flush()
        raise SystemExit
    
elif "linux" in platform_name:
    sys.stdout.write("Linux operating system detected\n")
    sys.stdout.flush()
    try:
        os.system((sys.argv[1]+" "+sys.argv[2]+" "+sys.argv[3]))
        sys.stdout.flush()
    except:
        sys.stdout.write("no syscall provided, exiting...\n")
        sys.stdout.flush()
        raise SystemExit
    
elif "darwin" in platform_name:
    sys.stdout.write("darwin operating system detected\n")
    sys.stdout.flush()
    try:
        os.system((sys.argv[1]+" "+sys.argv[2]+" "+sys.argv[3]))
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
