Import("env")
import stlink

env.Replace(UPLOADCMD=stlink.on_upload)
