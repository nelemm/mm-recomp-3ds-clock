// Horloge façon Majora's Mask 3D : une barre des 3 jours en bas de l'écran, un curseur qui
// avance avec le temps, une bulle avec le soleil/la lune et l'heure, et le compte à rebours
// avec le visage de la lune pendant les 6 dernières heures.
//
// Interface_DrawClock est remplacée : on garde ses conditions d'affichage et son fondu
// d'origine, mais au lieu de dessiner l'horloge N64 on met à jour un calque recompui.

#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "recompui.h"
#include "assets.h"


// ---------------------------------------------------------------------------------------------
// Mise en page (en dp, repère : coin haut gauche du calque de l'horloge)
// ---------------------------------------------------------------------------------------------
#define ROOT_W 800.0f
#define ROOT_H 172.0f
#define ROOT_BOTTOM 6.0f

#define BAR_X 40.0f
#define BAR_Y 144.0f
#define BAR_W 720.0f
#define BAR_H 22.0f
#define SEG_W (BAR_W / 3.0f)

#define CURSOR_W 20.0f
#define CURSOR_H 11.0f

#define BUBBLE_W 150.0f
#define BUBBLE_H 42.0f
#define BUBBLE_Y (BAR_Y - CURSOR_H - 2.0f - BUBBLE_H)
#define BUBBLE_ICON 28.0f
#define BUBBLE_DIGIT_H 26.0f

#define ARROW_H 20.0f

#define FINAL_FACE 46.0f
#define FINAL_DIGIT_H 36.0f

#define SWIRL_W 160.0f
#define ORN_W 130.0f

#define FINAL_HOURS_DIGITS 8 // HH:MM:SS
#define BUBBLE_DIGITS 5      // H:MM ou HH:MM

#define TIME_PER_DAY 0x10000

// Pulsation de la bulle : un cycle complet (transparent puis opaque) par minute de jeu.
// Calée sur l'heure du jeu, elle ralentit en temps ralenti et suit les mods de vitesse du temps.
#define PULSE_MIN_OPACITY 0.45f

typedef enum { DIGITS_NORMAL, DIGITS_SLOW, DIGITS_FINAL, DIGITS_SETS } DigitSet;

// ---------------------------------------------------------------------------------------------
// État du calque
// ---------------------------------------------------------------------------------------------
static RecompuiContext sContext = RECOMPUI_NULL_CONTEXT;
static RecompuiResource sRoot;
static RecompuiResource sSegments[3];
static RecompuiResource sCursor;
static RecompuiResource sBubble;
static RecompuiResource sBubbleIcon;
static RecompuiResource sBubbleDigits[BUBBLE_DIGITS];
static RecompuiResource sArrow;
static RecompuiResource sFinalGroup;
static RecompuiResource sFinalDigits[FINAL_HOURS_DIGITS];

static RecompuiTextureHandle sTexSeg[3], sTexSegDim[3];
static RecompuiTextureHandle sTexDigits[DIGITS_SETS][11]; // 0-9, 10 = ':'
static f32 sDigitAspect[11];
static RecompuiTextureHandle sTexSun, sTexMoon, sTexMoonFace, sTexCursor, sTexArrow;
static RecompuiTextureHandle sTexSwirlL, sTexSwirlR, sTexOrn;

static bool sVisible = false;
static bool sDrawnThisFrame = false;

// Dernières valeurs envoyées à l'interface, pour ne la modifier que si besoin.
static s32 sShownDay = -1;
static s32 sShownDigits[FINAL_HOURS_DIGITS];
static s32 sShownMode = -1;
static f32 sShownCursorX = -1.0f;
static f32 sShownOpacity = -1.0f;
static f32 sShownPulse = -1.0f;

static const RecompuiColor sColorClear = { 0, 0, 0, 0 };
static const RecompuiColor sColorWhite = { 255, 255, 255, 255 };
static const RecompuiColor sColorBubble = { 8, 8, 10, 225 };
static const RecompuiColor sColorGold = { 222, 182, 60, 255 };

#define TEX(name) recompui_create_texture_rgba32(tex_##name, TEX_##name##_W, TEX_##name##_H)

static void load_textures(void) {
    sTexSeg[0] = TEX(day1);
    sTexSeg[1] = TEX(day2);
    sTexSeg[2] = TEX(day3);
    sTexSegDim[0] = TEX(day1_dim);
    sTexSegDim[1] = TEX(day2_dim);
    sTexSegDim[2] = TEX(day3_dim);

#define DIGIT_SET(set, prefix)                                       \
    sTexDigits[set][0] = TEX(prefix##_0);                            \
    sTexDigits[set][1] = TEX(prefix##_1);                            \
    sTexDigits[set][2] = TEX(prefix##_2);                            \
    sTexDigits[set][3] = TEX(prefix##_3);                            \
    sTexDigits[set][4] = TEX(prefix##_4);                            \
    sTexDigits[set][5] = TEX(prefix##_5);                            \
    sTexDigits[set][6] = TEX(prefix##_6);                            \
    sTexDigits[set][7] = TEX(prefix##_7);                            \
    sTexDigits[set][8] = TEX(prefix##_8);                            \
    sTexDigits[set][9] = TEX(prefix##_9);                            \
    sTexDigits[set][10] = TEX(prefix##_colon);
    DIGIT_SET(DIGITS_NORMAL, dg)
    DIGIT_SET(DIGITS_SLOW, dc)
    DIGIT_SET(DIGITS_FINAL, do)
#undef DIGIT_SET

    sDigitAspect[0] = (f32)TEX_DG_0_W / TEX_DG_0_H;
    sDigitAspect[1] = (f32)TEX_DG_1_W / TEX_DG_1_H;
    sDigitAspect[2] = (f32)TEX_DG_2_W / TEX_DG_2_H;
    sDigitAspect[3] = (f32)TEX_DG_3_W / TEX_DG_3_H;
    sDigitAspect[4] = (f32)TEX_DG_4_W / TEX_DG_4_H;
    sDigitAspect[5] = (f32)TEX_DG_5_W / TEX_DG_5_H;
    sDigitAspect[6] = (f32)TEX_DG_6_W / TEX_DG_6_H;
    sDigitAspect[7] = (f32)TEX_DG_7_W / TEX_DG_7_H;
    sDigitAspect[8] = (f32)TEX_DG_8_W / TEX_DG_8_H;
    sDigitAspect[9] = (f32)TEX_DG_9_W / TEX_DG_9_H;
    sDigitAspect[10] = (f32)TEX_DG_COLON_W / TEX_DG_COLON_H;

    sTexSun = TEX(sun);
    sTexMoon = TEX(moon);
    sTexMoonFace = TEX(moonface);
    sTexCursor = TEX(cursor);
    sTexArrow = TEX(arrow);
    sTexSwirlL = TEX(swirl_l);
    sTexSwirlR = TEX(swirl_r);
    sTexOrn = TEX(orn);
}

static RecompuiResource place_image(RecompuiResource parent, RecompuiTextureHandle tex, f32 x, f32 y, f32 w, f32 h) {
    RecompuiResource img = recompui_create_imageview(sContext, parent, tex);
    recompui_set_position(img, POSITION_ABSOLUTE);
    recompui_set_left(img, x, UNIT_DP);
    recompui_set_top(img, y, UNIT_DP);
    recompui_set_width(img, w, UNIT_DP);
    recompui_set_height(img, h, UNIT_DP);
    return img;
}

static RecompuiResource place_box(RecompuiResource parent, f32 x, f32 y, f32 w, f32 h, const RecompuiColor* bg) {
    RecompuiResource box = recompui_create_element(sContext, parent);
    recompui_set_position(box, POSITION_ABSOLUTE);
    recompui_set_left(box, x, UNIT_DP);
    recompui_set_top(box, y, UNIT_DP);
    recompui_set_width(box, w, UNIT_DP);
    recompui_set_height(box, h, UNIT_DP);
    recompui_set_background_color(box, bg);
    return box;
}

static RecompuiResource flex_digit(RecompuiResource parent, f32 height) {
    RecompuiResource img = recompui_create_imageview(sContext, parent, sTexDigits[DIGITS_NORMAL][0]);
    recompui_set_height(img, height, UNIT_DP);
    recompui_set_width(img, height * sDigitAspect[0], UNIT_DP);
    return img;
}

static void create_ui(void) {
    load_textures();

    sContext = recompui_create_context();
    recompui_set_context_captures_input(sContext, 0);
    recompui_set_context_captures_mouse(sContext, 0);
    recompui_open_context(sContext);

    RecompuiResource screen = recompui_context_root(sContext);
    sRoot = recompui_create_element(sContext, screen);
    recompui_set_position(sRoot, POSITION_ABSOLUTE);
    recompui_set_left(sRoot, 50.0f, UNIT_PERCENT);
    recompui_set_margin_left(sRoot, -ROOT_W / 2.0f, UNIT_DP);
    recompui_set_bottom(sRoot, ROOT_BOTTOM, UNIT_DP);
    recompui_set_width(sRoot, ROOT_W, UNIT_DP);
    recompui_set_height(sRoot, ROOT_H, UNIT_DP);

    // Arabesques de chaque côté et ornement central
    f32 swirlH = SWIRL_W * TEX_SWIRL_L_H / TEX_SWIRL_L_W;
    place_image(sRoot, sTexSwirlL, BAR_X - SWIRL_W * 0.62f, BAR_Y + BAR_H - swirlH + 4.0f, SWIRL_W, swirlH);
    place_image(sRoot, sTexSwirlR, BAR_X + BAR_W - SWIRL_W * 0.38f, BAR_Y + BAR_H - swirlH + 4.0f, SWIRL_W, swirlH);
    f32 ornH = ORN_W * TEX_ORN_H / TEX_ORN_W;
    place_image(sRoot, sTexOrn, (ROOT_W - ORN_W) / 2.0f, BAR_Y - ornH + 3.0f, ORN_W, ornH);

    // Les 3 jours
    for (s32 i = 0; i < 3; i++) {
        sSegments[i] = place_image(sRoot, sTexSegDim[i], BAR_X + SEG_W * i, BAR_Y, SEG_W, BAR_H);
    }

    // Repères : séparations entre les jours et petits traits toutes les 6 h
    for (s32 i = 1; i < 3; i++) {
        place_box(sRoot, BAR_X + SEG_W * i - 1.0f, BAR_Y, 2.0f, BAR_H, &sColorWhite);
    }
    for (s32 i = 0; i < 12; i++) {
        if ((i % 4) != 0) {
            place_box(sRoot, BAR_X + (BAR_W / 12.0f) * i - 0.5f, BAR_Y + BAR_H - 6.0f, 1.0f, 6.0f, &sColorWhite);
        }
    }

    // Cadre blanc autour de la barre
    RecompuiResource frame = place_box(sRoot, BAR_X - 2.0f, BAR_Y - 2.0f, BAR_W + 4.0f, BAR_H + 4.0f, &sColorClear);
    recompui_set_border_width(frame, 2.0f, UNIT_DP);
    recompui_set_border_color(frame, &sColorWhite);
    recompui_set_border_radius(frame, 2.0f, UNIT_DP);

    // Curseur
    sCursor = place_image(sRoot, sTexCursor, BAR_X - CURSOR_W / 2.0f, BAR_Y - CURSOR_H - 1.0f, CURSOR_W, CURSOR_H);

    // Bulle : icône + heure
    sBubble = place_box(sRoot, 0.0f, BUBBLE_Y, BUBBLE_W, BUBBLE_H, &sColorBubble);
    recompui_set_border_radius(sBubble, 10.0f, UNIT_DP);
    recompui_set_border_width(sBubble, 2.0f, UNIT_DP);
    recompui_set_border_color(sBubble, &sColorClear);
    recompui_set_display(sBubble, DISPLAY_FLEX);
    recompui_set_flex_direction(sBubble, FLEX_DIRECTION_ROW);
    recompui_set_align_items(sBubble, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(sBubble, JUSTIFY_CONTENT_CENTER);
    recompui_set_gap(sBubble, 1.0f, UNIT_DP);
    sBubbleIcon = recompui_create_imageview(sContext, sBubble, sTexSun);
    recompui_set_width(sBubbleIcon, BUBBLE_ICON, UNIT_DP);
    recompui_set_height(sBubbleIcon, BUBBLE_ICON, UNIT_DP);
    recompui_set_margin_right(sBubbleIcon, 8.0f, UNIT_DP);
    for (s32 i = 0; i < BUBBLE_DIGITS; i++) {
        sBubbleDigits[i] = flex_digit(sBubble, BUBBLE_DIGIT_H);
    }

    // Flèche du temps ralenti, accolée à droite de la bulle
    f32 arrowW = ARROW_H * TEX_ARROW_W / TEX_ARROW_H;
    sArrow = place_image(sRoot, sTexArrow, 0.0f, BUBBLE_Y + (BUBBLE_H - ARROW_H) / 2.0f, arrowW, ARROW_H);
    recompui_set_display(sArrow, DISPLAY_NONE);

    // 6 dernières heures : visage de la lune + compte à rebours
    sFinalGroup = recompui_create_element(sContext, sRoot);
    recompui_set_position(sFinalGroup, POSITION_ABSOLUTE);
    recompui_set_left(sFinalGroup, 0.0f, UNIT_DP);
    recompui_set_top(sFinalGroup, 0.0f, UNIT_DP);
    recompui_set_width(sFinalGroup, ROOT_W, UNIT_DP);
    recompui_set_flex_direction(sFinalGroup, FLEX_DIRECTION_COLUMN);
    recompui_set_align_items(sFinalGroup, ALIGN_ITEMS_CENTER);
    recompui_set_display(sFinalGroup, DISPLAY_NONE);
    RecompuiResource face = recompui_create_imageview(sContext, sFinalGroup, sTexMoonFace);
    recompui_set_width(face, FINAL_FACE, UNIT_DP);
    recompui_set_height(face, FINAL_FACE, UNIT_DP);
    RecompuiResource row = recompui_create_element(sContext, sFinalGroup);
    recompui_set_display(row, DISPLAY_FLEX);
    recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
    recompui_set_align_items(row, ALIGN_ITEMS_CENTER);
    for (s32 i = 0; i < FINAL_HOURS_DIGITS; i++) {
        sFinalDigits[i] = flex_digit(row, FINAL_DIGIT_H);
    }

    recompui_set_display(sRoot, DISPLAY_NONE);
    recompui_close_context(sContext);
    recompui_show_context(sContext);
}

// Affiche un chiffre (0-9, 10 = ':', -1 = caché) dans un emplacement.
static void set_digit(RecompuiResource slot, s32 value, DigitSet set, f32 height) {
    if (value < 0) {
        recompui_set_display(slot, DISPLAY_NONE);
        return;
    }
    recompui_set_display(slot, DISPLAY_BLOCK);
    recompui_set_imageview_texture(slot, sTexDigits[set][value]);
    recompui_set_width(slot, height * sDigitAspect[value], UNIT_DP);
}

typedef enum { MODE_NORMAL, MODE_SLOW, MODE_FINAL } ClockMode;

static void update_ui(f32 opacity, f32 pulse) {
    u16 time = CURRENT_TIME;
    s32 day = CURRENT_DAY;
    if (day < 1) {
        day = 1;
    } else if (day > 3) {
        day = 3;
    }

    bool finalHours = (CURRENT_DAY >= 4) ||
                      ((CURRENT_DAY == 3) && (time >= (CLOCK_TIME(0, 0) + 5)) && (time < CLOCK_TIME(6, 0)));
    ClockMode mode = finalHours ? MODE_FINAL
                     : (gSaveContext.save.timeSpeedOffset == -2) ? MODE_SLOW
                                                                  : MODE_NORMAL;

    // Position sur les 3 jours : chaque jour va de 6:00 à 6:00.
    u32 elapsed = (u32)(day - 1) * TIME_PER_DAY + (u16)(time - CLOCK_TIME(6, 0));
    f32 frac = (f32)elapsed / (3.0f * TIME_PER_DAY);
    s32 untilCrash = TIME_UNTIL_MOON_CRASH;
    if (untilCrash < 0) {
        untilCrash = 0;
    }
    if (finalHours) {
        frac = 1.0f - (f32)untilCrash / (3.0f * TIME_PER_DAY);
    }
    frac = CLAMP(frac, 0.0f, 1.0f);
    f32 cursorX = BAR_X + BAR_W * frac;

    s32 digits[FINAL_HOURS_DIGITS];
    for (s32 i = 0; i < FINAL_HOURS_DIGITS; i++) {
        digits[i] = -1;
    }
    bool isDay = true;
    if (mode == MODE_FINAL) {
        // Vraies secondes de jeu restantes avant la chute de la lune.
        // 86400 s / 0x10000 unités = 675 / 512 ; untilCrash <= 0x10000 en fin de cycle, donc pas de débordement.
        u32 secs = ((u32)untilCrash * 675u) / 512u;
        u32 h = secs / 3600, m = (secs / 60) % 60, s = secs % 60;
        if (h > 99) {
            h = 99;
        }
        digits[0] = h / 10; digits[1] = h % 10; digits[2] = 10;
        digits[3] = m / 10; digits[4] = m % 10; digits[5] = 10;
        digits[6] = s / 10; digits[7] = s % 10;
    } else {
        u32 minutes = ((u32)time * (24 * 60)) / TIME_PER_DAY;
        u32 hour24 = minutes / 60, minute = minutes % 60;
        u32 hour12 = hour24 % 12;
        if (hour12 == 0) {
            hour12 = 12;
        }
        isDay = (hour24 >= 6) && (hour24 < 18);
        s32 i = 0;
        if (hour12 >= 10) {
            digits[i++] = hour12 / 10;
        }
        digits[i++] = hour12 % 10;
        digits[i++] = 10;
        digits[i++] = minute / 10;
        digits[i++] = minute % 10;
        digits[FINAL_HOURS_DIGITS - 1] = isDay; // mémorisé pour détecter le changement d'icône
    }

    bool digitsChanged = false;
    for (s32 i = 0; i < FINAL_HOURS_DIGITS; i++) {
        if (digits[i] != sShownDigits[i]) {
            digitsChanged = true;
        }
    }
    bool changed = digitsChanged || ((s32)mode != sShownMode) || (day != sShownDay) ||
                   (cursorX != sShownCursorX) || (opacity != sShownOpacity) || (pulse != sShownPulse) || !sVisible;
    if (!changed) {
        return;
    }

    recompui_open_context(sContext);

    if (!sVisible) {
        recompui_set_display(sRoot, DISPLAY_BLOCK);
        sVisible = true;
    }
    if (opacity != sShownOpacity) {
        recompui_set_opacity(sRoot, opacity);
        sShownOpacity = opacity;
    }
    if (pulse != sShownPulse) {
        recompui_set_opacity(sBubble, pulse);
        recompui_set_opacity(sArrow, pulse);
        sShownPulse = pulse;
    }
    if (day != sShownDay) {
        for (s32 i = 0; i < 3; i++) {
            recompui_set_imageview_texture(sSegments[i], (i == day - 1) ? sTexSeg[i] : sTexSegDim[i]);
        }
        sShownDay = day;
    }
    if (cursorX != sShownCursorX) {
        recompui_set_left(sCursor, cursorX - CURSOR_W / 2.0f, UNIT_DP);
        f32 bubbleX = CLAMP(cursorX - BUBBLE_W / 2.0f, 0.0f, ROOT_W - BUBBLE_W);
        recompui_set_left(sBubble, bubbleX, UNIT_DP);
        recompui_set_left(sArrow, bubbleX + BUBBLE_W - 2.0f, UNIT_DP);
        sShownCursorX = cursorX;
    }
    if ((s32)mode != sShownMode) {
        recompui_set_display(sBubble, (mode == MODE_FINAL) ? DISPLAY_NONE : DISPLAY_FLEX);
        recompui_set_display(sFinalGroup, (mode == MODE_FINAL) ? DISPLAY_FLEX : DISPLAY_NONE);
        recompui_set_display(sArrow, (mode == MODE_SLOW) ? DISPLAY_BLOCK : DISPLAY_NONE);
        recompui_set_border_color(sBubble, (mode == MODE_SLOW) ? &sColorGold : &sColorClear);
    }
    if (digitsChanged || ((s32)mode != sShownMode)) {
        if (mode == MODE_FINAL) {
            for (s32 i = 0; i < FINAL_HOURS_DIGITS; i++) {
                set_digit(sFinalDigits[i], digits[i], DIGITS_FINAL, FINAL_DIGIT_H);
            }
        } else {
            DigitSet set = (mode == MODE_SLOW) ? DIGITS_SLOW : DIGITS_NORMAL;
            for (s32 i = 0; i < BUBBLE_DIGITS; i++) {
                set_digit(sBubbleDigits[i], digits[i], set, BUBBLE_DIGIT_H);
            }
            recompui_set_imageview_texture(sBubbleIcon, isDay ? sTexSun : sTexMoon);
        }
        for (s32 i = 0; i < FINAL_HOURS_DIGITS; i++) {
            sShownDigits[i] = digits[i];
        }
        sShownMode = (s32)mode;
    }

    recompui_close_context(sContext);
}

static void hide_ui(void) {
    if (sVisible && sContext != RECOMPUI_NULL_CONTEXT) {
        recompui_open_context(sContext);
        recompui_set_display(sRoot, DISPLAY_NONE);
        recompui_close_context(sContext);
        sVisible = false;
    }
}

// Onde triangulaire sur la minute de jeu en cours : 1 -> PULSE_MIN_OPACITY -> 1.
static f32 bubble_pulse(PlayState* play) {
    // Position dans la minute : (heure x 1440) modulo une journée (0x10000 unités).
    u32 inMinute = ((u32)(u16)CURRENT_TIME * (24u * 60u)) % TIME_PER_DAY;
    f32 phase = (f32)inMinute / (f32)TIME_PER_DAY;                       // 0..1
    f32 tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);    // 0 -> 1 -> 0
    return 1.0f - tri * (1.0f - PULSE_MIN_OPACITY);
}

// ---------------------------------------------------------------------------------------------
// Remplacement de l'horloge d'origine
// ---------------------------------------------------------------------------------------------
RECOMP_PATCH void Interface_DrawClock(PlayState* play) {
    static s16 sThreeDayClockAlpha = 255;
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    // Mêmes conditions d'affichage que l'horloge d'origine.
    if ((R_TIME_SPEED != 0) &&
        ((msgCtx->msgMode == MSGMODE_NONE) ||
         ((play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) && !Play_InCsMode(play)) ||
         ((msgCtx->currentTextId >= 0x100) && (msgCtx->currentTextId <= 0x200)) ||
         (gSaveContext.gameMode == GAMEMODE_END_CREDITS)) &&
        !FrameAdvance_IsEnabled(play) && !Environment_IsTimeStopped() && (gSaveContext.save.day <= 3)) {

        // Fondu du HUD d'origine (cinématiques, etc.), sans l'effacement quand Link bouge.
        sThreeDayClockAlpha = (play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) ? 255 : interfaceCtx->bAlpha;

        if (!IS_PAUSED(&play->pauseCtx) && (sThreeDayClockAlpha > 0)) {
            if (sContext == RECOMPUI_NULL_CONTEXT) {
                create_ui();
            }
            update_ui(sThreeDayClockAlpha / 255.0f, bubble_pulse(play));
            sDrawnThisFrame = true;
        }
    }
}

// Interface_DrawClock n'est appelée que depuis Interface_Draw : si elle ne l'a pas été pendant
// cette image (horloge masquée, pause, menu...), on cache le calque.
RECOMP_HOOK("Interface_Draw")
void Clock3DS_OnInterfaceDraw(PlayState* play) {
    sDrawnThisFrame = false;
}

RECOMP_HOOK_RETURN("Interface_Draw")
void Clock3DS_OnInterfaceDrawReturn(void) {
    if (!sDrawnThisFrame) {
        hide_ui();
    }
}

// En quittant le jeu (retour à l'écran titre, etc.), Interface_Draw n'est plus appelée.
RECOMP_HOOK("Play_Destroy")
void Clock3DS_OnPlayDestroy(GameState* thisx) {
    hide_ui();
}
