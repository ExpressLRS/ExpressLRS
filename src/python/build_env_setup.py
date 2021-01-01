Import("env", "projenv")
import stlink
import UARTupload
import opentx
import upload_via_esp8266_backpack

stm = env.get('PIOPLATFORM', '') in ['ststm32']

# don't overwrite if custom command defined
if stm and "$UPLOADER $UPLOADERFLAGS" in env.get('UPLOADCMD', '$UPLOADER $UPLOADERFLAGS'):
    target_name = env['PIOENV'].upper()
    print("STM ENv: '%s'" % target_name)
    if "_R9M_" in target_name:
        env.AddPostAction("buildprog", [opentx.gen_elrs])
        if "WIFI" in target_name:
            env.Replace(UPLOADCMD=upload_via_esp8266_backpack.on_upload)
        else:
            env.Replace(UPLOADCMD=stlink.on_upload)
    elif "_STLINK" in target_name:
        env.Replace(UPLOADCMD=stlink.on_upload)
    elif "_BETAFLIGHTPASSTHROUGH" in target_name:
        env.Replace(UPLOADCMD=UARTupload.on_upload)
