Import("env", "projenv")
import stlink
import UARTupload
import opentx
import upload_via_esp8266_backpack

stm = env.get('PIOPLATFORM', '') in ['ststm32']

# don't overwrite if custom command defined
if stm and "$UPLOADER $UPLOADERFLAGS" in env.get('UPLOADCMD', '$UPLOADER $UPLOADERFLAGS'):
    target_name = env['PIOENV'].upper()
    print("BUILD ENV: '%s'" % target_name)
    if target_name == "FRSKY_TX_R9M_VIA_STLINK_OLD_BOOTLOADER_DEPRECATED":
        print("you are using the old bootloader, please update this will be removed soon")
        env.AddPostAction("buildprog", [opentx.gen_frsky])
    elif "_R9M_" in target_name or "ES915TX" in target_name:
        env.AddPostAction("buildprog", [opentx.gen_elrs])
        if "WIFI" in target_name:
            env.Replace(UPLOADCMD=upload_via_esp8266_backpack.on_upload)
        else:
            env.Replace(UPLOADCMD=stlink.on_upload)
    elif "_STLINK" in target_name:
        env.Replace(UPLOADCMD=stlink.on_upload)
    elif "_BETAFLIGHTPASSTHROUGH" in target_name:
        env.Replace(UPLOADCMD=UARTupload.on_upload)
    elif "_DFU" in target_name:
        board = env.BoardConfig()
        # PIO's ststm32 forces stm32f103 to upload via maple_upload,
        # but we really actually truly want dfu-util
        env.Replace(UPLOADER="dfu-util", UPLOADERFLAGS=["-d", "0483:df11",
            "-s", "%s:leave" % board.get("upload.offset_address", "0x08001000"),
            "-R", "-D"],
            UPLOADCMD='$UPLOADER $UPLOADERFLAGS "${SOURCE.get_abspath()}"')
