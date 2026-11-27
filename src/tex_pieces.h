#ifndef CLOCK3DS_TEX_PIECES_H
#define CLOCK3DS_TEX_PIECES_H

// Pièces de l'horloge découpées par la bibliothèque native (native/clock3ds_textures.cpp) dans
// l'atlas UI du pack MM3D 4K. Partagé par le mod (C, MIPS) et la DLL (C++) : l'ordre compte.

// Fichier de l'atlas UI dans le pack MM3D 4K (cherché dans le dossier indiqué dans les réglages), et sa taille attendue.
#define CLOCK3DS_ATLAS_FILE "tex1_512x256_45D746278BB3DA8A_13_mip0.png"
#define CLOCK3DS_ATLAS_W 4096
#define CLOCK3DS_ATLAS_H 2048

#define CLOCK3DS_DIGIT_SETS 3    // normal (crème), temps ralenti (cyan), compte à rebours (orange)
#define CLOCK3DS_DIGIT_GLYPHS 11 // 0-9, 10 = ':'

enum Clock3dsTexPiece {
    TP_DAY1,
    TP_DAY2,
    TP_DAY3,
    TP_DAY1_DIM,
    TP_DAY2_DIM,
    TP_DAY3_DIM,
    TP_DIGITS, // TP_DIGITS + set * CLOCK3DS_DIGIT_GLYPHS + glyph
    TP_SUN = TP_DIGITS + CLOCK3DS_DIGIT_SETS * CLOCK3DS_DIGIT_GLYPHS,
    TP_MOON,
    TP_CURSOR,
    TP_ARROW,
    TP_SWIRL_L,
    TP_SWIRL_R,
    TP_ORN,
    TP_COUNT
};

// Longueur maximale du chemin saisi dans les réglages (octets, zéro final compris).
#define CLOCK3DS_PATH_MAX 1024

// Taille maximale d'une pièce (en pixels), pour borner les copies.
#define CLOCK3DS_MAX_PIECE_PIXELS (512 * 256)

#endif
