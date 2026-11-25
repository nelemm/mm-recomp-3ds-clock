# 3DS Clock

A [Zelda 64: Recompiled](https://github.com/Zelda64Recomp/Zelda64Recomp) mod for *Majora's Mask* that replaces the N64 three-day clock with the clock from *Majora's Mask 3D*.

[English](#english) · [Français](#français)

---

## English

### Features

- **Three-day bar** at the bottom of the screen, with the current day highlighted and the other days dimmed.
- **Cursor** that moves along the bar from 6:00 on the first day to 6:00 after the final day.
- **Time bubble** above the cursor, with the sun (6:00–18:00) or the moon and the time in 12-hour format. The bubble gently pulses once per in-game minute, so it follows the speed of time.
- **Slowed time** (Inverted Song of Time): gold outline, cyan digits and the slowed-time arrow, as on 3DS.
- **Final hours**: the moon's face and an `HH:MM:SS` countdown (real in-game seconds) replace the bubble.
- Same visibility rules as the original clock (hidden during dialogue, pause, stopped time, after day 3) and same HUD fades.

To get the original clock back, disable the mod.

### Textures (optional, not included)

The mod builds and works **without any texture**: the clock is then drawn with plain shapes and text (coloured day bars, cursor, bubble, sun/moon dots, coloured digits).

For the real 3DS look, the pieces are cut from the *Majora's Mask 3D* interface as upscaled by Henriko Magnifico's **MM3D 4K** texture pack. Those images are not redistributable, so you generate them from your own copy of the pack. Only **one file** of the pack is used (about 2 MB), and only at build time: the pieces are embedded in the `.nrm`, so the pack is not needed to play.

1. Copy the pack's UI atlas into this repository as `source/ui_atlas.png` (ignored by git):
   ```
   <pack>/user/load/textures/0004000000125500/UI/tex1_512x256_45D746278BB3DA8A_13_mip0.png
   ```
2. Generate the pieces (Python 3 and [Pillow](https://pypi.org/project/pillow/) required):
   ```bash
   pip install pillow
   python tools/gen_assets.py
   ```

This writes `assets/*.rgba` (raw RGBA32 pieces at half the pack's resolution), `assets/preview.png` (a contact sheet) and `src/assets.h`. When `src/assets.h` exists, the build uses the textures automatically.

### Building

Requirements: `make`, a MIPS-capable `clang` and `ld.lld`, and `RecompModTool` from [N64Recomp](https://github.com/N64Recomp/N64Recomp).

```bash
git clone --recurse-submodules <this repository>
cd mm-recomp-3ds-clock
python tools/gen_assets.py   # optional, see Textures
make
RecompModTool mod.toml build
```

Copy `build/nelem_mm_clock_3ds.nrm` into your mods folder (`%LOCALAPPDATA%\Zelda64Recompiled\mods` on Windows) and enable **3DS Clock** in the **Mods** menu.

### Compatibility

- Replaces `Interface_DrawClock` with `RECOMP_PATCH`: not compatible with another mod that patches the same function.
- Works with the Archipelago randomizer ([MMRecompRando](https://github.com/RecompRando/MMRecompRando)) and with time-speed mods: everything is computed from the in-game time.
- Requires Zelda 64: Recompiled 1.2.2 or newer.

### How it works

- `Interface_DrawClock` keeps the original visibility conditions and HUD alpha, but updates a `recompui` overlay instead of drawing the N64 clock.
- With textures, they are created once with `recompui_create_texture_rgba32` from the embedded RGBA32 data. Without them (`__has_include("assets.h")` fails), the same layout is built from `recompui` boxes and labels.
- The overlay is only updated when a value changes.
- `Interface_Draw` and `Play_Destroy` hooks hide the overlay whenever the clock isn't drawn (pause, menus, title screen).

### License

Code: [CC0 1.0](LICENSE) — public domain. Based on the [MMRecompModTemplate](https://github.com/Zelda64Recomp/MMRecompModTemplate) (CC0) and the [zeldaret/mm](https://github.com/zeldaret/mm) decompilation.
The generated textures are derived from Nintendo's artwork and Henriko Magnifico's texture pack and are **not** covered by this license.

---

## Français

### Fonctionnalités

- **Barre des trois jours** en bas de l'écran : le jour en cours en pleine couleur, les autres assombris.
- **Curseur** qui avance de 6:00 le premier jour jusqu'à 6:00 après le dernier jour.
- **Bulle de l'heure** au-dessus du curseur : soleil (6:00–18:00) ou lune, heure au format 12 h. La bulle pulse doucement une fois par minute de jeu, elle suit donc la vitesse du temps.
- **Temps ralenti** (Chant du Temps inversé) : contour doré, chiffres cyan et flèche, comme sur 3DS.
- **Dernières heures** : visage de la lune et compte à rebours `HH:MM:SS` (vraies secondes de jeu) à la place de la bulle.
- Mêmes règles d'affichage que l'horloge d'origine (cachée pendant les dialogues, la pause, le temps arrêté, après le jour 3) et mêmes fondus du HUD.

Pour retrouver l'horloge d'origine, désactive le mod.

### Textures (facultatives, non incluses)

Le mod se compile et fonctionne **sans aucune texture** : l'horloge est alors dessinée avec des formes et du texte simples.

Pour le vrai rendu 3DS, les pièces viennent de l'interface de *Majora's Mask 3D* en haute résolution (pack **MM3D 4K** de Henriko Magnifico), qui ne peut pas être redistribué : tu les génères depuis ta propre copie. **Un seul fichier** du pack sert (environ 2 Mo), et seulement à la compilation : les pièces sont intégrées au `.nrm`, le pack n'est pas nécessaire pour jouer.

1. Copie l'atlas UI du pack dans ce dépôt sous le nom `source/ui_atlas.png` (ignoré par git) :
   `<pack>/user/load/textures/0004000000125500/UI/tex1_512x256_45D746278BB3DA8A_13_mip0.png`
2. Lance `python tools/gen_assets.py` (Python 3 et Pillow).

Si `src/assets.h` existe, la compilation utilise automatiquement les textures.

### Compilation

Voir la section [Building](#building) ci-dessus.

### Compatibilité

- Remplace `Interface_DrawClock` (`RECOMP_PATCH`) : incompatible avec un autre mod qui modifie la même fonction.
- Compatible avec le randomizer Archipelago et avec les mods qui changent la vitesse du temps.
- Nécessite Zelda 64: Recompiled 1.2.2 ou plus récent.

### Licence

Code : [CC0 1.0](LICENSE) — domaine public. Les textures générées dérivent des graphismes de Nintendo et du pack de Henriko Magnifico, et ne sont **pas** couvertes par cette licence.
