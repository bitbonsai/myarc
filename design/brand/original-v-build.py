#!/usr/bin/env python3
"""Original V study: the myArc arch with the twisted crossing ribbon removed.

Source-based edit of design/brand/assets/myarc-matte.png in its own 1254-pixel
space. Visible arch and leg pixels are kept as they are. The silhouette is read
from the original alpha on every scanline where the ribbon does not touch the
edge; the four occluded edge spans are bridged with cubic Hermite curves whose
end slopes come from the neighbouring clean rows. Leg pixels hidden under the
front band and the two curls are reconstructed by interpolating along the leg,
per lateral position, between the nearest visible rows. Those regions are a
reconstruction, not extracted pixels; the mask PNG and the study page show them.

Run from repository root. Needs Python 3 with Pillow and numpy."""
import base64, io, json, pathlib
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parents[2]
BRAND = ROOT / "design/brand"
ASSETS = BRAND / "assets"
OUT = BRAND / "exports/original-v"
OUT.mkdir(parents=True, exist_ok=True)
ORIGINAL = ASSETS / "myarc-matte.png"

src = Image.open(ORIGINAL).convert("RGBA")
A = np.array(src).astype(np.float64)
RGB, AL = A[..., :3], A[..., 3]
H, W = AL.shape
THR = 127.5

# Scanline ranges where the original alpha edge is the V's own edge. Elsewhere
# the ribbon front band or a curl touches that edge and the trace is bridged.
CLEAN = {
    "out_l": lambda y: y <= 440 or y >= 805,
    "in_l": lambda y: y <= 565 or y >= 915,
    "in_r": lambda y: y <= 625 or y >= 975,
    "out_r": lambda y: y <= 520 or y >= 880,
}
# Rows where the whole leg width is free of ribbon (colour model anchors).
FULL_CLEAN = {"left": lambda y: y <= 440 or y >= 860, "right": lambda y: y <= 525 or y >= 975}
RIBBON = {"left": lambda G: (G[..., 1] > 100) & (G[..., 0] < 110), "right": lambda G: G[..., 2] > 155}


def cross_up(row, start, step):
    """Sub-pixel x where alpha first rises through THR walking from start."""
    x = start
    while 0 <= x < W and row[x] < THR:
        x += step
    if not (0 <= x < W):
        return None
    a1, a0 = row[x], row[x - step]
    return x - step * (a1 - THR) / (a1 - a0) + 0.5


def cross_down(row, start, step):
    """Sub-pixel x where alpha first falls through THR walking from start."""
    x = start
    while 0 <= x < W and row[x] >= THR:
        x += step
    if not (0 <= x < W):
        return None
    a1, a0 = row[x], row[x - step]
    return x - step * (THR - a1) / (a0 - a1) + 0.5


def runs(row):
    """Opaque runs of a scanline as sub-pixel (start, end) pairs."""
    on = row >= THR
    if not on.any():
        return []
    d = np.diff(on.astype(int)); starts = list(np.where(d == 1)[0] + 1); ends = list(np.where(d == -1)[0])
    if on[0]: starts = [0] + starts
    if on[-1]: ends = ends + [W - 1]
    res = []
    for s, e in zip(starts, ends):
        res.append((cross_up(row, s, 1) if s > 0 else 0.0, cross_down(row, e, 1) if e < W - 1 else float(W)))
    return res


rows_on = [y for y in range(H) if (AL[y] >= THR).any()]
y_top, y_bot = rows_on[0], rows_on[-1]
out_l = np.full(H, np.nan); out_r = np.full(H, np.nan)
in_l = np.full(H, np.nan); in_r = np.full(H, np.nan)
# Arch: one run until the inner apex opens two runs wider than 50 px each.
y = y_top; prev = None
while True:
    rs = runs(AL[y])
    wide = [r for r in rs if r[1] - r[0] > 50]
    if prev is not None and len(wide) >= 2 and all(prev[0] - 2 <= r[0] and r[1] <= prev[1] + 2 for r in wide[:2]):
        break
    r = max(rs, key=lambda r: r[1] - r[0]); out_l[y], out_r[y] = r; prev = r; y += 1
y_hole = y


def track(y0, c0, slope0, pick):
    """Follow one leg down the image. The run containing the predicted centre
    is the leg; the centre prediction only learns from plausibly narrow runs."""
    hist = [(y0 - 1, c0)]; slope = slope0; res = {}
    for yy in range(y0, H):
        c_pred = hist[-1][1] + slope * (yy - hist[-1][0])
        rs = [r for r in runs(AL[yy]) if r[0] - 1 <= c_pred <= r[1] + 1]
        if not rs:
            break
        r = pick(rs, c_pred); res[yy] = r
        if r[1] - r[0] < 330:
            hist.append((yy, (r[0] + r[1]) / 2))
            if len(hist) > 40:
                ys_, cs_ = zip(*hist[-40:]); slope = np.polyfit(ys_, cs_, 1)[0]
    return res


first = sorted([r for r in runs(AL[y_hole]) if r[1] - r[0] > 50])
L0, R0 = first[0], first[-1]
left = track(y_hole, (L0[0] + L0[1]) / 2, -0.5, lambda rs, c: min(rs, key=lambda r: abs((r[0]+r[1])/2 - c)))
right = track(y_hole, (R0[0] + R0[1]) / 2, 0.42, lambda rs, c: min(rs, key=lambda r: abs((r[0]+r[1])/2 - c)))
for yy, (s, e) in left.items():
    out_l[yy], in_l[yy] = s, e
for yy, (s, e) in right.items():
    in_r[yy], out_r[yy] = s, e
yb_l, yb_r = max(left), max(right)


def bridge(arr, clean, y0, y1):
    """Keep clean rows, bridge every unclean span with a cubic Hermite curve."""
    out = arr.copy()
    ys = np.arange(y0, y1 + 1)
    ok = np.array([clean(y) and not np.isnan(arr[y]) for y in ys])
    for y in ys[~ok]:
        out[y] = np.nan
    # spans of NaN inside [y0, y1]
    y = y0
    while y <= y1:
        if np.isnan(out[y]):
            s = y
            while y <= y1 and np.isnan(out[y]):
                y += 1
            e = y - 1
            if s == y0 or e == y1:
                continue
            fit = lambda a, b: np.polyfit(np.arange(a, b + 1), out[a:b + 1], 1)[0]
            m0 = fit(max(y0, s - 41), s - 1); m1 = fit(e + 1, min(y1, e + 41))
            p0, p1 = out[s - 1], out[e + 1]
            L = (e + 1) - (s - 1)
            for yy in range(s, e + 1):
                t = (yy - (s - 1)) / L
                h00 = 2*t**3 - 3*t**2 + 1; h10 = t**3 - 2*t**2 + t
                h01 = -2*t**3 + 3*t**2; h11 = t**3 - t**2
                out[yy] = h00*p0 + h10*m0*L + h01*p1 + h11*m1*L
        else:
            y += 1
    return out, [(int(a), int(b)) for a, b in _spans(~ok, y0)]


def _spans(mask, y0):
    res = []; i = 0
    while i < len(mask):
        if mask[i]:
            j = i
            while j < len(mask) and mask[j]:
                j += 1
            res.append((y0 + i, y0 + j - 1)); i = j
        else:
            i += 1
    return res


OUT_L, sp_ol = bridge(out_l, CLEAN["out_l"], y_top, yb_l)
OUT_R, sp_or = bridge(out_r, CLEAN["out_r"], y_top, yb_r)
IN_L, sp_il = bridge(in_l, CLEAN["in_l"], y_hole, yb_l)
IN_R, sp_ir = bridge(in_r, CLEAN["in_r"], y_hole, yb_r)

# Silhouette polygon: down the left outer edge, up the left inner edge to the
# inner apex, down the right inner edge, up the right outer edge.
poly = []
for y in range(y_top, yb_l + 1): poly.append((OUT_L[y], y + 0.5))
for y in range(yb_l, y_hole - 1, -1): poly.append((IN_L[y], y + 0.5))
for y in range(y_hole, yb_r + 1): poly.append((IN_R[y], y + 0.5))
for y in range(yb_r, y_top - 1, -1): poly.append((OUT_R[y], y + 0.5))
S = 4
big = Image.new("L", (W*S, H*S), 0)
ImageDraw.Draw(big).polygon([(x*S, y*S) for x, y in poly], fill=255)
M = np.array(big.resize((W, H), Image.BOX)).astype(np.float64)
Mimg = Image.fromarray(M.astype(np.uint8))
Mexp = np.array(Mimg.filter(ImageFilter.MaxFilter(7))).astype(np.float64)   # +3 px
Mero = np.array(Mimg.filter(ImageFilter.MinFilter(5))).astype(np.float64)   # -2 px

# Zones. Z_edge: the original alpha edge is kept. Elsewhere the traced edge rules.
yy, xx = np.mgrid[0:H, 0:W]
mid = np.where(np.isnan(IN_L) | np.isnan(IN_R), 600.0, (np.nan_to_num(IN_L, nan=600) + np.nan_to_num(IN_R, nan=600)) / 2)
left_leg = (yy >= y_hole) & (xx < mid[yy])
right_leg = (yy >= y_hole) & ~left_leg
arch = yy < y_hole
cl = np.array([CLEAN["out_l"](y) for y in range(H)]); cil = np.array([CLEAN["in_l"](y) for y in range(H)])
cr = np.array([CLEAN["out_r"](y) for y in range(H)]); cir = np.array([CLEAN["in_r"](y) for y in range(H)])
lmid = (np.nan_to_num(OUT_L, nan=0) + np.nan_to_num(IN_L, nan=0)) / 2
rmid = (np.nan_to_num(OUT_R, nan=0) + np.nan_to_num(IN_R, nan=0)) / 2
z_edge = arch.copy()
z_edge |= left_leg & (xx < lmid[yy]) & cl[yy]
z_edge |= left_leg & (xx >= lmid[yy]) & cil[yy]
z_edge |= right_leg & (xx >= rmid[yy]) & cr[yy]
z_edge |= right_leg & (xx < rmid[yy]) & cir[yy]
alpha = np.where(z_edge, AL * Mexp / 255.0, M)


def bilinear(img, xs, ys):
    x0 = np.clip(np.floor(xs).astype(int), 0, W - 2); y0 = np.clip(np.floor(ys).astype(int), 0, H - 2)
    fx = np.clip(xs - x0, 0, 1)[..., None]; fy = np.clip(ys - y0, 0, 1)[..., None]
    return ((img[y0, x0]*(1-fx) + img[y0, x0+1]*fx)*(1-fy) + (img[y0+1, x0]*(1-fx) + img[y0+1, x0+1]*fx)*fy)


def reconstruct_leg(name, o_edge, i_edge, yb, leg_mask):
    """Rectify the leg to (row, lateral) space, find ribbon-covered samples by
    distance from a baseline interpolated between fully clean rows, then fill
    them along the leg from the nearest visible rows."""
    N = 320
    ys = np.arange(y_hole, yb + 1)
    u = (np.arange(N) + 0.5) / N
    X = o_edge[ys][:, None] + u[None, :] * (i_edge[ys] - o_edge[ys])[:, None]
    Y = np.repeat((ys + 0.5)[:, None], N, 1)
    G = bilinear(RGB, X, Y - 0.5)
    inside = bilinear(np.dstack([M, AL]), X, Y - 0.5)
    inside = (inside[..., 0] > 250) & (inside[..., 1] > 250)
    full = np.array([FULL_CLEAN[name](y) for y in ys])

    def fill(valid):
        F = G.copy(); good = []
        for j in range(N):
            v = valid[:, j]
            if v.sum() < 0.3 * len(ys):
                continue
            good.append(j)
            for c in range(3):
                F[:, j, c] = np.interp(ys, ys[v], G[v, j, c])
        for j in range(N):  # rim columns with too few visible samples copy the nearest well-anchored one
            if j not in good:
                F[:, j] = F[:, min(good, key=lambda g: abs(g - j))]
        # per-column anchors differ slightly; a 7-sample box across the width removes streaks
        pad = np.pad(F, ((0, 0), (3, 3), (0, 0)), mode="edge")
        return sum(pad[:, k:k + N] for k in range(7)) / 7.0
    # Ribbon over the leg differs in one channel. Light blue front band over the
    # blue-violet leg: G >= 107 with R < 100, while leg pixels in that span have
    # G <= 95 or, in the violet upper part, R >= 139. Magenta band and salmon
    # curl over the coral leg: B >= 158, leg highlights stay at B <= 144.
    occ = (~full[:, None]) & RIBBON[name](G)
    occ_img = Image.fromarray((occ * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(9))
    occ = np.array(occ_img) > 0
    F = fill(~occ & inside)
    # map back to pixels
    sel = leg_mask & (M > 0)
    py, px = np.where(sel)
    o = o_edge[py]; i = i_edge[py]
    uu = (px + 0.5 - o) / (i - o)
    ok = ~np.isnan(uu)
    py, px, uu = py[ok], px[ok], uu[ok]
    gy = np.clip(py - y_hole, 0, len(ys) - 1)
    gj = np.clip(np.round(uu * N - 0.5).astype(int), 0, N - 1)
    is_occ = occ[gy, gj] | ((~z_edge[py, px]) & ((Mero[py, px] < 250) | (AL[py, px] < 250)))
    col = F[gy, gj]
    return py[is_occ], px[is_occ], col[is_occ], (ys, X, Y, occ)


res = RGB.copy()
recon = np.zeros((H, W), bool)
for name, oe, ie, yb, lm in (("left", OUT_L, IN_L, yb_l, left_leg), ("right", IN_R, OUT_R, yb_r, right_leg)):
    py, px, col, _ = reconstruct_leg(name, oe, ie, yb, lm)
    res[py, px] = col; recon[py, px] = True
recon &= alpha > 0

final = np.dstack([res, alpha]).clip(0, 255).astype(np.uint8)
# clear colour where fully transparent
final[final[..., 3] == 0, :3] = 0
master = Image.fromarray(final, "RGBA")
master.save(ASSETS / "original-v-master.png")
Image.fromarray(M.astype(np.uint8)).save(ASSETS / "original-v-mask.png")
json.dump({
    "space": "myarc-matte.png pixels, x at row centre y+0.5",
    "bridged_spans": {"out_l": sp_ol, "in_l": sp_il, "in_r": sp_ir, "out_r": sp_or},
    "rows": {str(y): [None if np.isnan(v) else round(float(v), 2) for v in (OUT_L[y], IN_L[y], IN_R[y], OUT_R[y])]
             for y in range(y_top, max(yb_l, yb_r) + 1)},
}, open(ASSETS / "original-v-edges.json", "w"), separators=(",", ":"))


def resize_rgba(img, size):
    """Premultiplied Lanczos resize so edges do not darken."""
    a = np.array(img).astype(np.float64)
    al = a[..., 3:4] / 255.0
    pm = [Image.fromarray((a[..., c:c+1] * al)[..., 0].astype(np.float32), "F").resize(size, Image.LANCZOS) for c in range(3)]
    aa = Image.fromarray(a[..., 3].astype(np.float32), "F").resize(size, Image.LANCZOS)
    aa = np.clip(np.array(aa), 0, 255)
    out = np.zeros(size[::-1] + (4,), np.uint8)
    with np.errstate(divide="ignore", invalid="ignore"):
        for c in range(3):
            v = np.clip(np.array(pm[c]) / np.where(aa > 0, aa / 255.0, 1), 0, 255)
            out[..., c] = np.where(aa > 0, v, 0)
    out[..., 3] = aa
    return Image.fromarray(out, "RGBA")


# Icon framing: V bounding box, 4 percent margin, centred in a square.
ys_, xs_ = np.where(final[..., 3] > 0)
x0, x1, y0, y1 = xs_.min(), xs_.max() + 1, ys_.min(), ys_.max() + 1
side = int(max(x1 - x0, y1 - y0) * 1.08)
cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
sq.alpha_composite(master, (side//2 - cx, side//2 - cy))
for s in (16, 24, 32, 128, 512):
    resize_rgba(sq, (s, s)).save(OUT / f"original-v-{s}.png")

# Study images. Comparisons sit on ivory so JPEG keeps the page small.
IVORY = (245, 242, 236)
def on_ivory(img, size):
    bg = Image.new("RGBA", img.size, IVORY + (255,)); bg.alpha_composite(img)
    return resize_rgba(bg, (size, size)).convert("RGB")
D = 640
on_ivory(src, D).save(OUT / "study-original.jpg", quality=86)
on_ivory(master, D).save(OUT / "study-v.jpg", quality=86)
# geometry panel: original ghosted, traced silhouette outline, reconstructed pixels tinted
ghost = np.array(src).astype(np.float64); ghost[..., 3] *= 0.28
geo = Image.new("RGBA", (W, H), IVORY + (255,))
geo.alpha_composite(Image.fromarray(ghost.astype(np.uint8), "RGBA"))
ring = (np.array(Image.fromarray(M.astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 127) & \
       (np.array(Image.fromarray(M.astype(np.uint8)).filter(ImageFilter.MinFilter(5))) < 128)
layer = np.zeros((H, W, 4), np.uint8)
layer[recon] = (255, 140, 0, 110)
layer[ring] = (11, 20, 32, 255)
geo.alpha_composite(Image.fromarray(layer, "RGBA"))
geo.convert("RGB").resize((D, D), Image.LANCZOS).save(OUT / "study-geometry.jpg", quality=86)

recon_px = int(recon.sum()); v_px = int((final[..., 3] > 127).sum())
print(f"y_top={y_top} y_hole={y_hole} tips=({yb_l},{yb_r}) bbox=({x0},{y0},{x1},{y1})")
print("bridged spans:", sp_ol, sp_il, sp_ir, sp_or)
print(f"reconstructed {recon_px} of {v_px} opaque V pixels = {100*recon_px/v_px:.1f}%")


# Study page: one mobile-width column, images embedded so the file is self-contained.
def b64(path):
    mime = "image/png" if path.suffix == ".png" else "image/jpeg"
    return f"data:{mime};base64," + base64.b64encode(path.read_bytes()).decode()

I = {n: b64(OUT / f"study-{n}.jpg") for n in ("original", "v", "geometry")}
P = {s: b64(OUT / f"original-v-{s}.png") for s in (16, 24, 32)}
spans = "; ".join(f"{k} rows {a}–{b}" for k, (a, b) in
                  (("left outer", sp_ol[0]), ("left inner", sp_il[0]), ("right inner", sp_ir[0]), ("right outer", sp_or[0])))
icons = lambda: "".join(f'<img src="{P[s]}" width="{s}" height="{s}" alt="">' for s in (16, 24, 32))
html = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>myArc original V study</title>
<style>
body{{margin:0;padding:16px;font:13px/1.45 -apple-system,system-ui,sans-serif;color:#0B1420;background:#fff;max-width:420px}}
h1{{font-size:15px;margin:0 0 4px}}h2{{font-size:12px;margin:14px 0 6px;color:#555;text-transform:uppercase;letter-spacing:.04em}}
p{{margin:0 0 8px;color:#444}}
.pair{{display:flex;gap:8px}}.pair figure{{flex:1;margin:0}}
figure img{{width:100%;height:auto;display:block;border-radius:8px}}
figcaption{{font-size:11px;color:#666;margin-top:3px}}
.geo img{{width:100%;display:block;border-radius:8px}}
.sw{{border-radius:8px;padding:10px 12px;display:flex;align-items:center;gap:14px;margin-bottom:8px}}
.ivory{{background:#F5F2EC}}.night{{background:#0B1420}}.night .lbl{{color:#B8BEC8}}
.lbl{{font-size:11px;color:#666;font-weight:600;min-width:120px}}
.icons{{display:flex;align-items:flex-end;gap:14px}}.icons img{{display:block}}
small{{display:block;color:#666;font-size:11px}}
code{{font-size:11px}}
</style></head><body>
<h1>Original V, loop removed</h1>
<p>Same arch as the logo. Pixels of the original <code>myarc-matte.png</code> are kept; only the twisted crossing ribbon and its two curls are gone. Study only, nothing live changes.</p>
<div class="pair">
 <figure><img src="{I['original']}" alt="Original myArc mark"><figcaption>Original</figcaption></figure>
 <figure><img src="{I['v']}" alt="Arch with ribbon removed"><figcaption>Ribbon removed</figcaption></figure>
</div>
<h2>Geometry change</h2>
<div class="geo"><img src="{I['geometry']}" alt="Original ghosted with traced V outline and reconstructed areas tinted"></div>
<small>Ink line: the V silhouette used. Orange: leg pixels that were under the ribbon and are reconstructed. Everything else is the original image.</small>
<h2>Actual size</h2>
<div class="sw ivory"><span class="lbl">Ivory · 16 / 24 / 32</span><div class="icons">{icons()}</div></div>
<div class="sw night"><span class="lbl">Midnight · 16 / 24 / 32</span><div class="icons">{icons()}</div></div>
<h2>How faithful</h2>
<p>Silhouette: read from the original alpha on every scanline where the ribbon does not touch the edge. Four edge spans were hidden and are bridged with cubic curves matched to the clean rows on both sides ({spans}). The right outer bridge sits inside the original silhouette on every row.</p>
<p>Colour: {100*recon_px/v_px:.0f}% of the V's area was under the front band or a curl. Those pixels are filled by interpolating along the leg between the nearest visible rows at the same position across the width. Real tube shading under the ribbon is unknown, so this part is a reconstruction, not extraction. Rounded tips, apex, and the leg parts visible beside the ribbon are untouched.</p>
<small>Files: <code>assets/original-v-master.png</code> (1254 px, transparent), <code>assets/original-v-mask.png</code>, <code>assets/original-v-edges.json</code>, <code>exports/original-v/original-v-16|24|32|128|512.png</code>. Rebuild: <code>python3 design/brand/original-v-build.py</code>.</small>
</body></html>"""
(BRAND / "original-v-study.html").write_text(html)
print("study page", BRAND / "original-v-study.html", f"{len(html)//1024} KB")
