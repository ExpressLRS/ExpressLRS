"""Drive a TX screen over USB serial (firmware built with -DDEBUG_SCREEN_REMOTE).

usage:
  remote.py PORT [--baud N] KEYS...   send keys, e.g. 'U' (long up), 'd', 'o'
  remote.py PORT shot NAME            dump the framebuffer to NAME.scr/.png
  remote.py PORT demo                 cycle the demo link state ('#')

Keys: u d l r o = short press, U D L R O = long press, ~ = demo on/off.
Several actions can be chained: remote.py COM26 U wait:0.5 d shot:menu1
"""
import sys, time, os, serial

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import render  # noqa: E402


def open_port(port, baud):
    s = serial.Serial()
    s.port = port
    s.baudrate = baud
    s.timeout = 0.2
    s.dtr = True   # S3 USB-Serial-JTAG drops TX without DTR; with RTS low this never resets
    s.rts = False
    s.open()
    return s


def read_until_end(s, timeout=5):
    buf = b""
    t0 = time.time()
    while time.time() - t0 < timeout:
        buf += s.read(65536)
        if b"END" in buf[-8:] or b"END\n" in buf:
            break
    return buf.decode("latin-1")


def shot(s, name):
    s.reset_input_buffer()
    s.write(b"!")
    text = read_until_end(s)
    w, h, bpp, rows, nrows, rb = render.parse(text, want_rows=True)
    for attempt in range(4):
        missing = [r for r in range(nrows) if r not in rows]
        if not missing:
            break
        for r in missing:
            s.write(b"@%03d" % r)
            t = read_until_end(s, 1)
            try:
                _, _, _, got, _, _ = render.parse(t, want_rows=True)
                rows.update(got)
            except ValueError:
                pass
    missing = [r for r in range(nrows) if r not in rows]
    data = b"".join(rows.get(r, bytes(rb)) for r in range(nrows))
    with open(name + ".scr", "w") as f:
        f.write("SCR %d %d %d\n" % (w, h, bpp))
        for r in range(nrows):
            f.write(data[r * rb:(r + 1) * rb].hex() + "\n")
        f.write("END\n")
    render.to_image(w, h, bpp, data).save(name + ".png")
    print(f"{name}.png  ({w}x{h}x{bpp})" + (f"  {len(missing)} rows missing" if missing else ""))


def main():
    args = sys.argv[1:]
    port = args.pop(0)
    baud = 460800
    if args and args[0] == "--baud":
        args.pop(0)
        baud = int(args.pop(0))
    s = open_port(port, baud)
    for a in args:
        if a.startswith("shot"):
            name = a.split(":", 1)[1] if ":" in a else "shot"
            shot(s, name)
        elif a.startswith("wait:"):
            time.sleep(float(a[5:]))
        elif a == "demo":
            s.write(b"#")
        else:
            for c in a:
                s.write(c.encode())
                time.sleep(0.12)  # one key per screen poll (20 ms) with margin
    s.close()


if __name__ == "__main__":
    main()
