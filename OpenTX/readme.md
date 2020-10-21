This directory contains specially buily openTX binaries which unlock 500hz undate rate on crsf serial output. It also has some improvements that improves RC packet jitter. 

These builds include crsfshot + ghost sync + internal module sync. This means you don't have to decide between having good internal module or external module performance as is currently the case with the crsfshot/ghost custom binaries (see https://github.com/opentx/opentx/issues/7644)
These features will not be available together until OpenTX 2.4, so consider this a very early preview. Don't bother the OpenTX devs about these builds are they will not be officially supported and will be deprecated as of OpenTX 2.4.
