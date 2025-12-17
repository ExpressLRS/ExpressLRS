import sys
import subprocess
import pkg_resources
import site

# list of packages to install
required_packages = {'pyserial', 'zopflipy'}

# Determine the correct python executable within the PIO virtual environment
python_exe = sys.executable

installed_packages = {pkg.key for pkg in pkg_resources.working_set}
missing_packages = required_packages - installed_packages

if missing_packages:
    print(f"Missing Python packages: {missing_packages}. Installing them now...")
    subprocess.check_call([python_exe, '-m', 'pip', 'install', *missing_packages, '--no-warn-script-location'])
    print("Packages installed.")
else:
    print("All required Python packages are already installed.")
