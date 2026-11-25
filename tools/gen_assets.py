"""Usage : python tools/gen_assets.py [chemin/vers/atlas.png]

Découpe les pièces de l'horloge 3DS dans l'atlas UI du pack MM3D 4K et produit :
- assets/*.rgba : pixels bruts RGBA32 (R,G,B,A octet par octet) pour recompui_create_texture_rgba32
- assets/preview.png : planche de contrôle
- src/assets.h : déclarations INCBIN + tailles
"""
import sys, pathlib
from PIL import Image, ImageOps, ImageFilter, ImageEnhance

Image.MAX_IMAGE_PIXELS = None
ROOT = pathlib.Path(__file__).resolve().parent.parent
# Atlas UI du pack MM3D 4K : par défaut source/ui_atlas.png (ignoré par git), ou chemin en argument.
ATLAS = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "source" / "ui_atlas.png"
if not ATLAS.is_file():
    sys.exit(f"Atlas introuvable : {ATLAS}. "
             "Copie tex1_512x256_45D746278BB3DA8A_13_mip0.png (dossier UI du pack MM3D 4K) "
             "dans source/ui_atlas.png, ou passe son chemin en argument.")
OUT = ROOT / "assets"
OUT.mkdir(parents=True, exist_ok=True)
SCALE = 0.5  # demi-résolution du pack 4K

im = Image.open(ATLAS).convert("RGBA")

def crop(box):
    return im.crop(box)

def shrink(img, scale=SCALE):
    w, h = img.size
    return img.resize((max(1, round(w * scale)), max(1, round(h * scale))), Image.LANCZOS)

def recolor_fill(img, color):
    """Teinte les pixels clairs (le remplissage crème) dans `color`, garde le contour sombre."""
    px = img.load()
    out = img.copy(); po = out.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            lum = (r + g + b) / 765.0
            t = max(0.0, min(1.0, (lum - 0.35) / 0.5))  # 0 = contour, 1 = remplissage
            if t > 0:
                cr, cg, cb = color
                po[x, y] = (round(r * (1 - t) + cr * t * (0.75 + 0.25 * lum)),
                            round(g * (1 - t) + cg * t * (0.75 + 0.25 * lum)),
                            round(b * (1 - t) + cb * t * (0.75 + 0.25 * lum)), a)
    return out

def outline(img, radius=3, color=(20, 10, 0)):
    a = img.getchannel("A").filter(ImageFilter.MaxFilter(radius * 2 + 1))
    base = Image.new("RGBA", img.size, color + (0,))
    base.putalpha(a)
    pad = Image.new("RGBA", img.size, (0, 0, 0, 0))
    pad.alpha_composite(base)
    pad.alpha_composite(img)
    return pad

def darken(img, f):
    rgb = ImageEnhance.Brightness(img.convert("RGB")).enhance(f).convert("RGBA")
    rgb.putalpha(img.getchannel("A"))
    return rgb

assets = {}

def add(name, img):
    assets[name] = img

# --- Barres des 3 jours (+ versions assombries pour les jours qui ne sont pas en cours)
bars = {"day1": (719, 450, 1201, 509), "day2": (719, 512, 1201, 571), "day3": (719, 575, 1201, 636)}
for k, box in bars.items():
    b = shrink(crop(box))
    add(k, b)
    add(k + "_dim", darken(b, 0.45))

# --- Chiffres 0-9 et deux-points (normal crème, cyan = temps ralenti, orange cerclé = compte à rebours)
runs = [(5, 60), (73, 110), (133, 188), (196, 251), (257, 317), (324, 380), (388, 443), (452, 507), (516, 571), (579, 634), (649, 665)]
names = [str(i) for i in range(10)] + ["colon"]
for (x0, x1), n in zip(runs, names):
    g = crop((x0 - 4, 584, x1 + 4, 692))
    add("dg_" + n, shrink(g))
    add("dc_" + n, shrink(recolor_fill(g, (90, 225, 255))))
    add("do_" + n, shrink(outline(recolor_fill(g, (255, 150, 20)), 5)))

# --- Icônes
add("sun", shrink(crop((578, 322, 672, 419))))
add("moon", shrink(crop((589, 456, 668, 540))))
face = crop((3460, 760, 3651, 957))
add("moonface", shrink(recolor_fill(face, (255, 130, 30))))
add("cursor", shrink(crop((1774, 577, 1815, 599))))
add("arrow", shrink(crop((2755, 1948, 2891, 2043))))

# --- Arabesques (droite = original, gauche = miroir) et ornement central
swirl = shrink(crop((2692, 19, 3132, 132)))
add("swirl_l", swirl)
add("swirl_r", ImageOps.mirror(swirl))
orn_strip = crop((3062, 1837, 4096, 1912))
# motif central : colonnes où l'ornement est le plus haut
a = orn_strip.getchannel("A").load()
heights = []
for x in range(orn_strip.width):
    top = next((y for y in range(orn_strip.height) if a[x, y] > 60), orn_strip.height)
    heights.append(orn_strip.height - top)
peak = max(range(len(heights)), key=lambda i: heights[i])
mid = peak  # sommet des boucles centrales
half = 150
add("orn", shrink(orn_strip.crop((max(0, mid - half), 0, min(orn_strip.width, mid + half), orn_strip.height))))

# --- Écriture
lines = ['#ifndef CLOCK3DS_ASSETS_H', '#define CLOCK3DS_ASSETS_H', '', '#include "incbin.h"', '']
total = 0
for n, img in assets.items():
    img = img.convert("RGBA")
    data = img.tobytes()  # R,G,B,A
    (OUT / f"{n}.rgba").write_bytes(data)
    total += len(data)
    lines.append(f'INCBIN(tex_{n}, "assets/{n}.rgba")')
    for nm in sorted({n, n.upper()}):
        lines.append(f'#define TEX_{nm}_W {img.width}')
        lines.append(f'#define TEX_{nm}_H {img.height}')
lines += ['', '#endif', '']
(ROOT / "src" / "assets.h").write_text("\n".join(lines), encoding="utf-8")

# --- Planche de contrôle
W = 1400
x = y = 10; rowh = 0
sheet = Image.new("RGBA", (W, 900), (40, 70, 40, 255))
for n, img in assets.items():
    if x + img.width + 10 > W:
        x = 10; y += rowh + 10; rowh = 0
    sheet.alpha_composite(img, (x, y))
    x += img.width + 10; rowh = max(rowh, img.height)
sheet.crop((0, 0, W, y + rowh + 10)).save(OUT / "preview.png")
print(f"{len(assets)} textures, {total // 1024} Ko de pixels")
