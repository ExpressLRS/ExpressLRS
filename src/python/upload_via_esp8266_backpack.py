import subprocess, os
import opentx

def process_http_result(output_json_file: str) -> bool:
    import json
    # Print the HTTP result that was saved to the json file
    # Returns: true if the result was OK
    with open(output_json_file) as f:
        output_json = json.load(f)

    result = output_json['status']
    msg = output_json['msg']
    retval = False

    if result == 'ok':
        # Update complete. Please wait for LED to resume blinking before disconnecting power.
        msg = f'UPLOAD SUCCESS\n\033[32m{msg}\033[0m'  # green
        # 'ok' is the only acceptable result
        retval = True
    elif result == 'mismatch':
        # <b>Current target:</b> LONG_ASS_NAME.<br><b>Uploaded image:</b> OTHER_NAME.<br/><br/>Flashing the wrong firmware may lock or damage your device.
        msg = msg.replace('<br>', '\n').replace('<br/>', '\n') # convert breaks to newline
        msg = msg.replace('<b>', '\033[34m').replace('</b>', '\033[0m') # bold to blue
        msg = '\033[33mTARGET MISMATCH\033[0m\n' + msg # yellow warning
    else:
        # Not enough space.
        msg = f'UPLOAD ERROR\n\033[31m{msg}\033[0m' # red

    print()
    print(msg, flush=True)
    return retval

def on_upload(source, target, env):
    isstm = env.get('PIOPLATFORM', '') in ['ststm32']
    bootloader_target = None

    upload_addr = ['elrs_tx', 'elrs_tx.local']
    app_start = 0 # eka bootloader offset

    # Parse upload flags:
    upload_flags = env.get('UPLOAD_FLAGS', [])
    for line in upload_flags:
        flags = line.split()
        for flag in flags:
            if "VECT_OFFSET=" in flag:
                offset = flag.split("=")[1]
                if "0x" in offset:
                    offset = int(offset, 16)
                else:
                    offset = int(offset, 10)
                app_start = offset
            if "BOOTLOADER=" in flag:
                bootloader_file = flag.split("=")[1]
                bootloader_target = os.path.join((env.get('PROJECT_DIR')), bootloader_file)


    firmware_path = str(source[0])
    bin_path = os.path.dirname(firmware_path)
    elrs_bin_target = os.path.join(bin_path, 'firmware.elrs')
    if not os.path.exists(elrs_bin_target):
        elrs_bin_target = os.path.join(bin_path, 'firmware.bin')
        if not os.path.exists(elrs_bin_target):
            raise Exception("No valid binary found!")

    bin_upload_output = os.path.splitext(elrs_bin_target)[0] + '-output.json'
    if os.path.exists(bin_upload_output):
        os.remove(bin_upload_output)

    cmd = ["curl", "--max-time", "60",
           "--retry", "2", "--retry-delay", "1",
           "-o", "%s" % (bin_upload_output)]

    pio_target = target[0].name
    uri = 'update'
    do_bin_upload = True

    if pio_target == 'uploadconfirm':
        uri = 'forceupdate?action=confirm'
        do_bin_upload = False
    if pio_target == 'uploadforce':
        cmd += ["-F", "force=1"]
    if do_bin_upload:
        cmd += "-F", "data=@%s" % (elrs_bin_target),
    if isstm:
        uri = 'upload'
        cmd += ["-F", "flash_address=0x%X" % (app_start,)]

    if bootloader_target is not None and isstm:
        cmd_bootloader = ["curl", "--max-time", "60",
            "--retry", "2", "--retry-delay", "1",
            "-F", "data=@%s" % (bootloader_target,), "-F", "flash_address=0x0000"]

    upload_port = env.get('UPLOAD_PORT', None)
    if upload_port is not None:
        upload_addr = [upload_port]

    returncode = -1
    for addr in upload_addr:
        addr = "http://%s/%s" % (addr, uri)
        print(" ** UPLOADING TO: %s" % addr)
        try:
            # Flash bootloader first if set
            if bootloader_target is not None:
                print("** Flashing Bootloader...")
                print(cmd_bootloader,cmd)
                subprocess.check_call(cmd_bootloader + [addr])
                print("** Bootloader Flashed!")
                print()

            # Flash main application binary
            subprocess.check_call(cmd + [addr])
            success = process_http_result(bin_upload_output)
            if success:
                # process_http_result should have printed whatever message
                # the target returned if it was successful
                return 0
        except subprocess.CalledProcessError as e:
            returncode = e.returncode

    print("WIFI upload FAILED!")
    return returncode
