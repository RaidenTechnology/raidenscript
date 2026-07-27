# Cover generators

Two covers, both produced by RaidenScript programs. Neither uses a graphics
library — there isn't one in the program, and there isn't one in the language.
What they have is what the language has: numbers, strings, lists, maps, loops.

| File | Output | Time |
|---|---|---|
| [`kapak-ascii.rai`](kapak-ascii.rai) | ASCII art — red bolt + wordmark. Prints the **text**, or a **PPM image** of that text. | ~6 s |
| [`kapak.rai`](kapak.rai) | A shaded 630×500 image, computed pixel by pixel. | ~45 s |

```bash
rai demo/kapak/kapak-ascii.rai > cover.ppm     # CIKTI = "ppm"  (default)
rai demo/kapak/kapak-ascii.rai                 # CIKTI = "metin" -> the text art
rai demo/kapak/kapak.rai       > gradient.ppm
```

The text output is checked in as [`cover.txt`](cover.txt).

## How the ASCII bolt is drawn

The bolt is a **seven-point polygon**, and "is this point inside?" is answered by
ray casting: walk the edges, count the crossings, odd means inside.

That test runs on a **character grid**, not on pixels. Each of the 105×58 cells
takes nine sub-samples, and the coverage (0–9) picks a character from a density
ramp:

```
" . : - = + * #"
```

So the shading is not anti-aliasing — it is choosing a heavier glyph where more
of the cell is covered. The same coverage number also picks the red: dark ember
at the edges, hot core in the middle.

The wordmark uses the 5×7 bitmap font at the bottom of the file, but stamped at
**character** resolution: every set pixel of a glyph becomes one `#` on the
grid. The subtitles are written straight in, one character per cell — at font
scale a 29-character line would need 174 columns and there are only 105, which
the first version discovered by running off the right edge.

## How the image comes out

In `"ppm"` mode the same grid is rendered with the same font, this time at pixel
resolution: 6×8 pixels per cell. Each pixel row expands the glyph row of every
cell once, then walks across — that keeps the map lookup out of the inner loop.

`kapak.rai` skips characters entirely and evaluates every pixel: three radial
falloffs for the mesh gradient, a modulo for the grid, the same polygon for the
bolt, and a rim light from sampling the polygon three pixels to each side.

## Turning a PPM into a PNG

Plain text in, PNG out, with the Python standard library alone:

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

## The one performance rule

Rows are collected in a list and joined once. Writing `row = row + pixel` in the
inner loop would be quadratic — measured 7.3× slower at 12,000 pieces, which at
these image sizes is the difference between a coffee break and a working script.
If you remember one thing about performance in this language, remember this one.
