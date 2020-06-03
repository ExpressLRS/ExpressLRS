Import("env")
env.Replace(UPLOADCMD="curl -v -F image=@$SOURCE $UPLOAD_PORT")