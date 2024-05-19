# Logos for TFT/OLED display

TFT color displays logos must be sized 160x53 in RGB565 raw format, so the file will be 16960 bytes in size.
```bash
ffmpeg -vcodec png -i input.png -vcodec rawvideo -f rawvideo -pix_fmt rgb565 logo.bin
```

Logos for display on the OLED screen must be size 128x64 for normal sized or 128x32 for small screen size.
OLED logos are in monochrome raw format.

Start with elrs.xcf (gimp file) add a logo to the right side of the ELRS logo, max height 50 pixels
starting at column 64. Once the image is correct, save as a BMP and execute the following command to get a
binary logo suitable for the flasher to use.

```bash
convert logo.bmp -monochrome -colors 2 -type bilevel -write MONO:logo.bin
```
