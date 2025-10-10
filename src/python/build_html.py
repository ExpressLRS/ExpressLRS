Import("env")
import subprocess, os, shutil

os.chdir('html')
subprocess.run(['npm', 'i'])

target_name = env['PIOENV'].upper()

my_env = os.environ.copy()
my_env["VITE_FEATURE_IS_TX"] = 'yes' if '_TX_' in target_name else 'no'
my_env["VITE_FEATURE_IS_8285"] = 'yes' if 'ESP8285' in env['PIOENV'] else 'no'
my_env["VITE_FEATURE_HAS_SX128X"] = 'yes' if '-DRADIO_SX128X=1' in env['BUILD_FLAGS'] else 'no'
my_env["VITE_FEATURE_HAS_SX127X"] = 'yes' if '-DRADIO_SX127X=1' in env['BUILD_FLAGS'] else 'no'
my_env["VITE_FEATURE_HAS_LR1121"] = 'yes' if '-DRADIO_LR1121=1' in env['BUILD_FLAGS'] else 'no'

subprocess.run(['npm', 'run', 'build'], env=my_env)
shutil.copy('dist/esp32_fs.h', '../include/WebContent.h')
