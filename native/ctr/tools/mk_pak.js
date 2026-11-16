// mk_pak.js: the Link screen's data for Tomb Raider Dual 3DS, made from the player's own disc with the app's own readers
// (app/www/trlevel.js, trmap.js, trbuild.js), so the 3DS map draws what the phone's does. Game data: it stays on the SD card.
//   node mk_pak.js <Track 01 .bin, .iso, .chd or the .cue> <out tomb-raider-link.pak>
// File: "TRPK", u32 version, u32 count, then count x { char name[24], u32 offset, u32 length }, then the blobs (little-endian):
//   lv<n>   one level: rooms (box, heights, flip, cuts, openings, the box under each sector, picture refs), boxes and what each
//           touches, items, secrets, exits. Every number s32 unless said.
//   px<n>   that level's pictures: RGB565, 0 = see-through (a real black is stored as 0x0020), 16 pixels per sector (the app's 32,
//           halved), north up; the refs in lv<n> point in here
//   names   level names, then the key item names per level
//   icons   pickup sprites (RGB565 as above)
//   font    the game's letters (sprite sequence 190) as rows of lit pixels, plus the punctuation trfont.js draws
const fs = require('fs'), path = require('path');
const www = path.join(__dirname, '..', '..', '..', 'app', 'www');
['trlevel.js', 'trmap.js', 'rnc.js', 'chd.js'].forEach(f => require(path.join(www, f)));
const TB = require(path.join(www, 'trbuild.js')), TRM = globalThis.TRM, TRL = globalThis.TRL;
const PAK_VERSION = 1;

let disc = process.argv[2], out = process.argv[3];
if (!disc || !out) { console.log('node mk_pak.js <disc .bin/.cue/.chd> <out.pak>'); process.exit(1); }
if (/\.cue$/i.test(disc)) {                                   // the first FILE of the cue sheet
  const m = /FILE\s+"([^"]+)"/i.exec(fs.readFileSync(disc, 'latin1'));
  if (!m) throw new Error('no FILE in ' + disc);
  disc = path.join(path.dirname(disc), m[1]);
}
const fd = fs.openSync(disc, 'r'), size = fs.statSync(disc).size;
const reader = (o, n) => { const b = Buffer.alloc(n); const k = fs.readSync(fd, b, 0, n, o); return Promise.resolve(new Uint8Array(b.buffer, b.byteOffset, k)); };

// ---- little writers
function W() {
  const parts = []; let n = 0;
  return {
    s32(v) { const b = Buffer.alloc(4); b.writeInt32LE(Math.round(v) | 0); parts.push(b); n += 4; },
    u16(v) { const b = Buffer.alloc(2); b.writeUInt16LE(v & 0xFFFF); parts.push(b); n += 2; },
    s16(v) { const b = Buffer.alloc(2); b.writeInt16LE(Math.max(-32768, Math.min(32767, v | 0))); parts.push(b); n += 2; },
    u8(v) { parts.push(Buffer.from([v & 255])); n++; },
    str(s, len) { const b = Buffer.alloc(len); b.write(String(s).slice(0, len - 1), 'latin1'); parts.push(b); n += len; },
    raw(b) { parts.push(Buffer.from(b)); n += b.length; },
    get n() { return n; },
    buf() { return Buffer.concat(parts); },
  };
}
const c565 = (r, g, b) => { const v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); return v || 0x0020; };
// RGBA (the app's pictures) -> RGB565 at half the size: a pixel is solid when at least 2 of its 4 are
function half(w, h, rgba) {
  const W2 = w >> 1, H2 = h >> 1, o = new Uint16Array(W2 * H2);
  for (let y = 0; y < H2; y++) for (let x = 0; x < W2; x++) {
    let r = 0, g = 0, b = 0, n = 0;
    for (let dy = 0; dy < 2; dy++) for (let dx = 0; dx < 2; dx++) {
      const i = ((2 * y + dy) * w + 2 * x + dx) * 4;
      if (rgba[i + 3]) { r += rgba[i]; g += rgba[i + 1]; b += rgba[i + 2]; n++; }
    }
    o[y * W2 + x] = n >= 2 ? c565(r / n, g / n, b / n) : 0;
  }
  return { w: W2, h: H2, px: o };
}
function full(w, h, rgba) {
  const o = new Uint16Array(w * h);
  for (let i = 0; i < w * h; i++) o[i] = rgba[4 * i + 3] ? c565(rgba[4 * i], rgba[4 * i + 1], rgba[4 * i + 2]) : 0;
  return { w, h, px: o };
}

// ---- the font: as trfont.js reads it (sequence 190: A-Z, a-z, 0-9, then punctuation), lit pixels only
function fontMap() {
  const m = {};
  for (let i = 0; i < 26; i++) { m[65 + i] = i; m[97 + i] = 26 + i; }
  for (let i = 0; i < 10; i++) m[48 + i] = 52 + i;
  const p = { '.': 62, ',': 63, '!': 64, '?': 65, '[': 66, '"': 67, '/': 68, '^': 69, '-': 71, '+': 72, ':': 73, ']': 75, '\\': 76, '`': 77, '#': 78, "'": 79 };
  for (const k in p) m[k.charCodeAt(0)] = p[k];
  return m;
}
// the punctuation the game hasn't got, drawn in its style (the same drawings as trfont.js's EXTRA); 0x7F is the middle dot
const EXTRA = { '(': ['..#', '.#.', '#..', '#..', '#..', '#..', '#..', '#..', '.#.', '..#'], ')': ['#..', '.#.', '..#', '..#', '..#', '..#', '..#', '..#', '.#.', '#..'],
  ';': ['##', '##', '..', '..', '..', '##', '##', '.#', '#.'], '=': ['#####', '.....', '#####'], '*': ['#.#.#', '.###.', '#.#.#'], '<': ['...#', '..#.', '.#..', '#...', '.#..', '..#.', '...#'], '>': ['#...', '.#..', '..#.', '...#', '..#.', '.#..', '#...'],
  '%': ['##...#', '##..#.', '...#..', '..#...', '.#....', '#...##', '....##'], '&': ['.##...', '#..#..', '#..#..', '.##...', '#.#..#', '#..##.', '#..#..', '.##.##'], '_': ['######'], '\x7f': ['##', '##'] };
const EXTRA_TOP = { '(': 10, ')': 10, ';': 7, '=': 6, '*': 10, '<': 8, '>': 8, '%': 9, '&': 9, '_': -1, '\x7f': 5 };
function fontBlob(L) {
  const q = L.sseq.find(s => s.type === 190); if (!q) return null;
  const m = fontMap(), G = {};
  for (const k in m) {
    const t = L.stex[q.start + m[k]]; if (!t) continue;
    const w = t.u1 - t.u0 + 1, h = t.v1 - t.v0 + 1, rows = []; let lo = w, hi = -1;
    for (let y = 0; y < h; y++) {
      const r = [];
      for (let x = 0; x < w; x++) {
        const c = TRL.texel(L, t.tile, t.clut, t.u0 + x, t.v0 + y);
        const on = !!c && ((c & 31) * 8 + ((c >> 5) & 31) * 8 + ((c >> 10) & 31) * 8) > 230;
        r.push(on); if (on) { lo = Math.min(lo, x); hi = Math.max(hi, x); }
      }
      rows.push(r);
    }
    if (hi < 0) { G[k] = { top: 0, w: 0, rows: [], adv: 6 }; continue; }
    G[k] = { top: -t.t, w: hi - lo + 1, rows: rows.map(r => r.slice(lo, hi + 1)), adv: hi - lo + 2 };
  }
  for (const k in EXTRA) { const rows = EXTRA[k].map(s => s.split('').map(c => c === '#')); G[k.charCodeAt(0)] = { top: EXTRA_TOP[k], w: rows[0].length, rows, adv: rows[0].length + 1 }; }
  G[32] = { top: 0, w: 0, rows: [], adv: 6 };
  // 128 glyphs: s8 top (pixels above the baseline of the first row), u8 w, u8 h, u8 advance, 16 x u16 rows (bit 0 = left pixel)
  const o = W();
  for (let c = 0; c < 128; c++) {
    const g = G[c];
    if (!g) { o.u8(0); o.u8(0); o.u8(0); o.u8(0); for (let i = 0; i < 16; i++) o.u16(0); continue; }
    o.u8(g.top & 255); o.u8(g.w); o.u8(Math.min(16, g.rows.length)); o.u8(g.adv);
    for (let i = 0; i < 16; i++) { let v = 0; const r = g.rows[i] || []; for (let x = 0; x < Math.min(16, g.w); x++) if (r[x]) v |= 1 << x; o.u16(v); }
  }
  return o.buf();
}

(async () => {
  const t0 = Date.now(), d = await TB.open(reader, size);
  const names = TB.names(await d.file(d.exe));
  const blobs = [];
  const icons = {};
  let font = null;
  for (let n = 0; n < 16; n++) {
    const fb = await d.file('PSXDATA/' + TB.FILES[n] + '.PSX'), L = TRL.parse(fb), o = TB.level(L);
    if (!font) font = fontBlob(L);
    const px = W(), lv = W();
    const pic = p => { const off = px.n; px.raw(Buffer.from(p.px.buffer, p.px.byteOffset, p.px.byteLength)); return [off, p.w, p.h]; };
    lv.s32(o.rooms.length);
    o.rooms.forEach((r, ri) => {
      const cuts = TRM.cuts(L.rooms[ri]);
      const rp = TRM.renderRoom(L, ri), pics = [pic(half(rp.w, rp.h, rp.rgba))];
      cuts.forEach(c => { const cp = TRM.renderRoom(L, ri, 0, c); pics.push(pic(half(cp.w, cp.h, cp.rgba))); });
      lv.s32(r.x); lv.s32(r.z); lv.s32(r.yt); lv.s32(r.yb); lv.s32(r.xs); lv.s32(r.zs); lv.s32(r.alt); lv.s32(r.f);
      lv.s32(cuts.length); cuts.forEach(c => lv.s32(c));
      lv.s32(r.p.length); r.p.forEach(p => p.forEach(v => lv.s32(v)));   // [room, x0, y0, z0, x1, y1, z1, normal y]
      r.sb.forEach(b => lv.s32(b));
      pics.forEach(p => { lv.s32(p[0]); lv.s32(p[1]); lv.s32(p[2]); });
    });
    lv.s32(o.boxes.length); o.boxes.forEach(b => b.forEach(v => lv.s32(v)));
    o.ovl.forEach(a => { lv.s32(a.length); a.forEach(v => lv.s32(v)); });
    lv.s32(o.items.length); o.items.forEach(it => it.forEach(v => lv.s32(v)));   // [type, room, x, y, z, has a sprite, flags]
    lv.s32(o.secrets.length); o.secrets.forEach(s => s.forEach(v => lv.s32(v)));  // [number, room, x, y, z]
    lv.s32(o.exit.length); o.exit.forEach(s => s.forEach(v => lv.s32(v)));       // [room, x, y, z]
    const bg = TB.floorTile(L);                                                    // the backdrop tile, full size
    if (bg) { const p = pic(full(bg.w, bg.h, bg.rgba)); lv.s32(p[0]); lv.s32(p[1]); lv.s32(p[2]); } else { lv.s32(0); lv.s32(0); lv.s32(0); }
    blobs.push(['lv' + n, lv.buf()], ['px' + n, px.buf()]);
    TB.KEY.concat([88, 89, 90, 91, 93, 94]).forEach(ty => { const own = TB.PERLEVEL.indexOf(ty) >= 0, k = own ? n + '_' + ty : String(ty); if (icons[k]) return; const ic = TB.icon(L, ty); if (ic) icons[k] = { lv: own ? n : -1, ty, p: full(ic.w, ic.h, ic.rgba) }; });
    console.log('level', n, names.levels[n], o.rooms.length, 'rooms', (lv.n / 1024).toFixed(0) + ' KB +', (px.n / 1048576).toFixed(2) + ' MB pictures');
  }
  const nm = W();
  names.levels.forEach(s => nm.str(s, 40)); for (let i = names.levels.length; i < 22; i++) nm.str('', 40);
  for (let n = 0; n < 16; n++) { const m = names.items[n] || {}, ks = Object.keys(m); nm.s32(ks.length); ks.forEach(k => { nm.s32(+k); nm.str(m[k], 40); }); }
  blobs.push(['names', nm.buf()]);
  const ic = W(), ik = Object.keys(icons); ic.s32(ik.length);
  ik.forEach(k => { const e = icons[k]; ic.s32(e.lv); ic.s32(e.ty); ic.s32(e.p.w); ic.s32(e.p.h); ic.raw(Buffer.from(e.p.px.buffer, e.p.px.byteOffset, e.p.px.byteLength)); if (e.p.px.byteLength & 3) ic.raw(Buffer.alloc(4 - (e.p.px.byteLength & 3))); });   // each icon padded to 4 bytes
  blobs.push(['icons', ic.buf()]);
  if (font) blobs.push(['font', font]);
  // the file
  const head = 12 + blobs.length * 32; let at = head; const h = W();
  h.raw(Buffer.from('TRPK', 'latin1')); h.s32(PAK_VERSION); h.s32(blobs.length);
  blobs.forEach(b => { h.str(b[0], 24); h.s32(at); h.s32(b[1].length); at += (b[1].length + 3) & ~3; });
  const outB = [h.buf()]; blobs.forEach(b => { outB.push(b[1]); const pad = ((b[1].length + 3) & ~3) - b[1].length; if (pad) outB.push(Buffer.alloc(pad)); });
  fs.writeFileSync(out, Buffer.concat(outB));
  console.log('ok', out, (at / 1048576).toFixed(1) + ' MB', ((Date.now() - t0) / 1000).toFixed(1) + ' s');
})().catch(e => { console.error('FAIL', e.stack); process.exit(1); });
