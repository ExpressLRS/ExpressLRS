# Logos for TFT/OLED display

TFT color displays logos must be sized 160x53 in RGB565 raw format, so the file will be 16960 bytes in size.
```bash
ffmpeg -vcodec png -i input.png -vcodec rawvideo -f rawvideo -pix_fmt rgb565 logo.bin
```

Logos for display on the OLED screen must be size 128x64 for normal sized or 128x32 for small screen size.
OLED logos are in monochrome raw format.

```bash
convert ~/Downloads/logo.bmp -monochrome -colors 2 -type bilevel -write MONO:logo.bin
```
