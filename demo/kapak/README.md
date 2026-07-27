# Cover generator

The cover image in the repository README is not a drawing. Every one of its
315,000 pixels is computed by [`kapak.rai`](kapak.rai), which prints a plain
PPM (P3) image to standard output.

```bash
rai demo/kapak/kapak.rai > cover.ppm
```

Takes about 45 seconds and produces a 630×500 image.

There is no graphics library involved — not in the program and not in the
language. What the program has is what RaidenScript has: numbers, strings,
lists, maps and loops.

## What is in there

| Part | How |
|---|---|
| Mesh gradient | three radial falloffs, squared distance, no `sqrt` needed |
| Grid | `x % 42 == 0 or y % 42 == 0` |
| Lightning bolt | a seven-point polygon, ray-casting point-in-polygon test |
| Bolt rim light | the same test sampled three pixels to each side |
| Text | a 5×7 bitmap font defined as data **inside the script** |

The font is the part worth reading. There is no system font available to a
sandboxed script, so the glyphs are lists of `"01110"` strings and
`metinBas()` stamps them into a mask at an integer scale.

## Turning it into a PNG

The PPM is a plain text file, so any converter works. With Python's standard
library alone, no dependencies:

```python
import zlib, struct
src = open('cover.ppm','rb').read().split()
w, h, vals = int(src[1]), int(src[2]), src[4:]
raw = bytearray()
i = 0
for y in range(h):
    raw.append(0)                      # PNG filter byte
    for x in range(w * 3):
        raw.append(int(vals[i])); i += 1
def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t+d) & 0xffffffff)
png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(bytes(raw), 9))
       + chunk(b'IEND', b''))
open('cover.png','wb').write(png)
```

## Note on strings

Rows are collected in a list and joined once. Writing `row = row + pixel` in the
inner loop would be quadratic — measured 7.3× slower at 12,000 pieces, which at
this image size is the difference between a coffee break and a working script.
This is the one performance rule of the language you actually have to know.
