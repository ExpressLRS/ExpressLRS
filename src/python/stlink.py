import os
import platform


def get_commands(env, firmware):
    #platform.platform()
    platform_name = platform.system().lower()

    BL_CMD = []
    APP_CMD = []

    flash_start = app_start = 0x08000000
    bootloader = None
    upload_flags = env.get('UPLOAD_FLAGS', [])

    for line in upload_flags:
        flags = line.split()
        for flag in flags:
            if "BOOTLOADER=" in flag:
                bootloader = flag.split("=")[1]
            elif "VECT_OFFSET=" in flag:
                offset = flag.split("=")[1]
                if "0x" in offset:
                    offset = int(offset, 16)
                else:
                    offset = int(offset, 10)
                app_start = flash_start + offset
    env_dir = env['PROJECT_PACKAGES_DIR']
    if "windows" in platform_name:
        TOOL = os.path.join(
            env_dir,
            "tool-stm32duino", "stlink", "ST-LINK_CLI.exe")
        TOOL = '"%s"' % TOOL
        if bootloader is not None:
            BL_CMD = [TOOL, "-c SWD SWCLK=8 -P",
                bootloader, hex(flash_start)]
        APP_CMD = [TOOL, "-c SWD SWCLK=8 -P",
            firmware, hex(app_start), "-RST"]
    elif "linux" in platform_name or "darwin" in platform_name:
        TOOL = os.path.join(
            env_dir,
            "tool-stm32duino", "stlink", "st-flash")
        if bootloader is not None:
            BL_CMD = [TOOL, "write", bootloader, hex(flash_start)]
        APP_CMD = [TOOL, "--reset", "write", firmware, hex(app_start)]
    elif "os x" in platform_name:
        print("OS X not supported at the moment\n")
        raise OSError
    else:
        print("Operating system: "+ platform_name +  " is not supported.\n")
        raise OSError

    return " ".join(BL_CMD), " ".join(APP_CMD)


def on_upload(source, target, env):
    firmware_path = str(source[0])

    BL_CMD, APP_CMD = get_commands(env, firmware_path)

    retval = 0
    # flash bootloader
    if BL_CMD:
        print("Cmd: {}".format(BL_CMD))
        retval = env.Execute(BL_CMD)
    # flash application
    if retval == 0 and APP_CMD:
        print("Cmd: {}".format(APP_CMD))
        retval = env.Execute(APP_CMD)
    return retval

