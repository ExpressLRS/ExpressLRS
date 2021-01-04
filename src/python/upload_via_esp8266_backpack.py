import subprocess, os
import opentx

def on_upload(source, target, env):
    # Make sure the firmware.elrs is generated
    opentx.gen_elrs(source, target, env)
    firmware_path = str(source[0])
    bin_path = os.path.dirname(firmware_path)
    elrs_bin_target = os.path.join(bin_path, 'firmware.elrs')

    cmd = ["curl", "-v", "--max-time", "60",
           "--retry", "2", "--retry-delay", "1",
           "-F", "data=@%s" % (elrs_bin_target,)]

    upload_addr = ['elrs_tx', 'elrs_tx.local']

    upload_port = env.get('UPLOAD_PORT', None)
    if upload_port is not None:
        upload_addr = [upload_port]

    for addr in upload_addr:
        addr = "http://%s/upload" % addr
        print(" ** UPLOADING TO: %s" % addr)
        try:
            subprocess.check_call(cmd + [addr])
            return
        except subprocess.CalledProcessError:
            print("FAILED!")

    raise Exception("WiFI upload FAILED!")
