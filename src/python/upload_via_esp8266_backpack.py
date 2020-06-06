Import("env")
try:
    env.Replace(UPLOADCMD="curl -v --max-time 60 --retry 2 --retry-delay 1 -F data=@$SOURCE http://elrs_tx/upload")
except:
    env.Replace(UPLOADCMD="curl -v --max-time 60 --retry 2 --retry-delay 1 -F data=@$SOURCE http://elrs_tx.local/upload")