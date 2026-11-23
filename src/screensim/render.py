"""Render .scr frame dumps (simulator or device '!' dumps) to PNG.

usage: render.py <in.scr|dir> [out_dir]   - writes <name>.png (x4 scale) and sheet.png
"""
import sys, os, glob, re
from PIL import Image, ImageDraw

OLED_ON = (150, 220, 255)  # SSD1306 "white" through a bluish lens
OLED_OFF = (6, 8, 12)

def parse(text, want_rows=False):
    """Parse a dump. Rows may be bare hex or "NNN:hex" (device dumps)."""
    lines = text.splitlines()
    for i, l in enumerate(lines):
        m = re.match(r"^SCR (128 64 1|160 80 16)\s*$", l)
        if m:
            w, h, bpp = map(int, m.group(1).split())
            row_bytes = w * 2 if bpp == 16 and w == 160 else 64
            row_bytes = 80 if bpp == 16 else 64
            nrows = (w * h * bpp // 8) // row_bytes
            rows = {}
            seq = 0
            for l2 in lines[i + 1:]:
                if l2.startswith("END"):
                    break
                l2 = l2.strip()
                idx = seq
                m2 = re.match(r"^(\d{3}):([0-9a-f]+)$", l2)
                if m2:
                    idx, l2 = int(m2.group(1)), m2.group(2)
                elif ":" in l2 or not re.match(r"^[0-9a-f]+$", l2):
                    continue
                seq += 1
                try:
                    b = bytes.fromhex(l2)
                except ValueError:
                    continue
                if len(b) == row_bytes:
                    rows[idx] = b
            if want_rows:
                return w, h, bpp, rows, nrows, row_bytes
            return w, h, bpp, b"".join(rows.get(r, bytes(row_bytes)) for r in range(nrows))
    # header lost in transit: infer the geometry from the most common row length
    import collections
    lens = collections.Counter(len(m.group(1)) for m in re.finditer(r"(?m)^\d{3}:([0-9a-f]+)$", text))
    if lens:
        L = lens.most_common(1)[0][0]
        if L in (128, 160):
            hdr = "SCR 128 64 1" if L == 128 else "SCR 160 80 16"
            return parse(hdr + chr(10) + text, want_rows)
    raise ValueError("no SCR header")

def to_image(w, h, bpp, data, scale=4):
    img = Image.new("RGB", (w * scale, h * scale), OLED_OFF)
    px = img.load()
    for y in range(h):
        for x in range(w):
            if bpp == 1:
                on = (data[(y // 8) * w + x] >> (y & 7)) & 1
                c = OLED_ON if on else OLED_OFF
            else:
                v = data[(y * w + x) * 2] | (data[(y * w + x) * 2 + 1] << 8)
                c = (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
            for dy in range(scale - (1 if bpp == 1 else 0)):
                for dx in range(scale - (1 if bpp == 1 else 0)):
                    px[x * scale + dx, y * scale + dy] = c
    return img

def main():
    src = sys.argv[1]
    files = sorted(glob.glob(os.path.join(src, "*.scr"))) if os.path.isdir(src) else [src]
    out = sys.argv[2] if len(sys.argv) > 2 else (src if os.path.isdir(src) else os.path.dirname(src) or ".")
    imgs = []
    for f in files:
        w, h, bpp, data = parse(open(f, encoding="latin-1").read())
        im = to_image(w, h, bpp, data)
        name = os.path.splitext(os.path.basename(f))[0]
        im.save(os.path.join(out, name + ".png"))
        imgs.append((name, im))
    if len(imgs) > 1:
        cols = 3
        W, H = imgs[0][1].size
        rows = (len(imgs) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * (W + 8), rows * (H + 22)), (40, 40, 40))
        d = ImageDraw.Draw(sheet)
        for i, (n, im) in enumerate(imgs):
            x, y = (i % cols) * (W + 8), (i // cols) * (H + 22)
            sheet.paste(im, (x, y + 18))
            d.text((x + 2, y + 3), n, fill=(230, 230, 230))
        sheet.save(os.path.join(out, "sheet.png"))
    print(f"rendered {len(imgs)} frames -> {out}")

if __name__ == "__main__":
    main()
