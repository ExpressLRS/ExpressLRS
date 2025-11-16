// Compact MD5 implementation extracted for reuse across the app.
// Returns a Uint8Array of 16 bytes (MD5 digest) encoded as bytes matching prior implementation.

const K = (() => {
  const k = [];
  for (let i = 0; i < 64;) {
    k[i] = 0 | (Math.abs(Math.sin(++i)) * 4294967296);
  }
  return k;
})();

export function calcMD5(str) {
  let b; let c; let d; let j;
  const x = [];
  const str2 = unescape(encodeURI(str));
  let a = str2.length;
  const h = [b = 1732584193, c = -271733879, ~b, ~c];
  let i = 0;

  for (; i <= a;) x[i >> 2] |= (str2.charCodeAt(i) || 128) << 8 * (i++ % 4);

  x[str = (a + 8 >> 6) * 16 + 14] = a * 8;
  i = 0;

  for (; i < str; i += 16) {
    a = h; j = 0;
    for (; j < 64;) {
      a = [
        d = a[3],
        ((b = a[1] | 0) +
          ((d = (
            (a[0] +
              [
                b & (c = a[2]) | ~b & d,
                d & b | ~d & c,
                b ^ c ^ d,
                c ^ (b | ~d)
              ][a = j >> 4]
            ) +
            (K[j] +
              (x[[
                j,
                5 * j + 1,
                3 * j + 5,
                7 * j
              ][a] % 16 + i] | 0)
            )
          )) << (a = [
            7, 12, 17, 22,
            5, 9, 14, 20,
            4, 11, 16, 23,
            6, 10, 15, 21
          ][4 * a + j++ % 4]) | d >>> 32 - a)
        ),
        b,
        c
      ];
    }
    for (j = 4; j;) h[--j] = h[j] + a[j];
  }

  const out = [];
  for (j = 0; j < 32;) out.push(((h[j >> 3] >> ((1 ^ j++ & 7) * 4)) & 15) * 16 + ((h[j >> 3] >> ((1 ^ j++ & 7) * 4)) & 15));

  return new Uint8Array(out);
}
