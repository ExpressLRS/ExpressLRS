Import("env", "projenv")
import stlink
import UARTupload
import opentx
import upload_via_esp8266_backpack
import esp_compress

platform = env.get('PIOPLATFORM', '')
stm = platform in ['ststm32']

target_name = env['PIOENV'].upper()
print("PLATFORM : '%s'" % platform)
print("BUILD ENV: '%s'" % target_name)

# don't overwrite if custom command defined
if stm and "$UPLOADER $UPLOADERFLAGS" in env.get('UPLOADCMD', '$UPLOADER $UPLOADERFLAGS'):
    if target_name == "FRSKY_TX_R9M_VIA_STLINK_OLD_BOOTLOADER_DEPRECATED":
        print("you are using the old bootloader, please update this will be removed soon")
        env.AddPostAction("buildprog", opentx.gen_frsky)
    elif "APLHA_900_TX" in target_name:
        env.Replace(UPLOADCMD=upload_via_esp8266_backpack.on_upload)
    elif "_R9M_" in target_name or "ES915TX" in target_name or "GHOST_2400_TX" in target_name or \
         "NAMIMNORC_VOYAGER" in target_name or "NAMIMNORC_FLASH" in target_name:
        env.AddPostAction("buildprog", opentx.gen_elrs)
        env.AddPreAction("upload", opentx.gen_elrs)
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
            "-D"], UPLOADCMD='$UPLOADER $UPLOADERFLAGS "${SOURCE.get_abspath()}"')
elif platform in ['espressif8266']:
    env.AddPostAction("buildprog", esp_compress.compressFirmware)
    env.AddPreAction("${BUILD_DIR}/spiffs.bin",
                     [esp_compress.compress_files])
    env.AddPreAction("${BUILD_DIR}/${ESP8266_FS_IMAGE_NAME}.bin",
                     [esp_compress.compress_files])
    env.AddPostAction("${BUILD_DIR}/${ESP8266_FS_IMAGE_NAME}.bin",
                     [esp_compress.compress_fs_bin])
    if "_WIFI" in target_name:
        env.Replace(UPLOAD_PROTOCOL="custom")
        env.Replace(UPLOADCMD=upload_via_esp8266_backpack.on_upload)
elif platform in ['espressif32'] and "_WIFI" in target_name:
    env.Replace(UPLOAD_PROTOCOL="custom")
    env.Replace(UPLOADCMD=upload_via_esp8266_backpack.on_upload)
