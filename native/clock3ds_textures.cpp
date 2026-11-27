// Bibliothèque native de 3DS Clock : lit l'atlas UI du pack MM3D 4K (chemin saisi par le joueur dans
// les réglages du mod) et en découpe les pièces de l'horloge au lancement du jeu. Aucune texture
// n'est distribuée avec le mod : sans le fichier, le mod dessine son horloge sans textures.
//
// Les traitements reproduisent ceux de tools/gen_assets.py (Pillow) : découpe, réduction de
// moitié (Lanczos), recoloration des chiffres, contour, assombrissement, miroir.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define CLOCK3DS_EXPORT extern "C" __declspec(dllexport)
#else
#include <sys/stat.h>
#define CLOCK3DS_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "stb_image.h"

#include "../src/tex_pieces.h"

// ------------------------------------------------------------------------------------------------
// Interface avec le runtime N64Recomp : f(rdram, ctx), arguments dans r4..r7, retour dans r2.
// ------------------------------------------------------------------------------------------------
struct RecompRegs {
    uint64_t r[32];
};

CLOCK3DS_EXPORT uint32_t recomp_api_version = 1;

static uint32_t arg_u32(void* ctx, int i) {
    return (uint32_t)((RecompRegs*)ctx)->r[4 + i];
}

static void ret_u32(void* ctx, uint32_t v) {
    ((RecompRegs*)ctx)->r[2] = (uint64_t)(int64_t)(int32_t)v;
}

// ------------------------------------------------------------------------------------------------
// Petite bibliothèque d'image RGBA 8 bits
// ------------------------------------------------------------------------------------------------
struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> px; // R,G,B,A

    Image() = default;
    Image(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_ * 4, 0) {}
    uint8_t* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
    const uint8_t* at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }
};

static uint8_t clamp_u8(double v) {
    v = std::nearbyint(v); // arrondi au pair le plus proche, comme round() de Python
    return (uint8_t)(v < 0.0 ? 0.0 : (v > 255.0 ? 255.0 : v));
}

static int py_round(double v) {
    return (int)std::nearbyint(v);
}

static Image crop(const Image& src, int x0, int y0, int x1, int y1) {
    Image out(x1 - x0, y1 - y0);
    for (int y = 0; y < out.h; y++) {
        for (int x = 0; x < out.w; x++) {
            int sx = x0 + x, sy = y0 + y;
            if (sx >= 0 && sy >= 0 && sx < src.w && sy < src.h) {
                std::memcpy(out.at(x, y), src.at(sx, sy), 4);
            }
        }
    }
    return out;
}

static double sinc(double x) {
    if (x == 0.0) {
        return 1.0;
    }
    x *= 3.14159265358979323846;
    return std::sin(x) / x;
}

static double lanczos3(double x) {
    return (x > -3.0 && x < 3.0) ? sinc(x) * sinc(x / 3.0) : 0.0;
}

// Rééchantillonnage d'un axe, sur des canaux en alpha prémultiplié (comme Pillow).
static std::vector<double> resample_axis(const std::vector<double>& in, int inW, int inH, int outLen, bool horizontal) {
    int inLen = horizontal ? inW : inH;
    int outW = horizontal ? outLen : inW;
    int outH = horizontal ? inH : outLen;
    std::vector<double> out((size_t)outW * outH * 4, 0.0);
    double scale = (double)inLen / outLen;
    double filterScale = scale < 1.0 ? 1.0 : scale;
    double support = 3.0 * filterScale;
    for (int o = 0; o < outLen; o++) {
        double center = (o + 0.5) * scale;
        int xmin = (int)(center - support + 0.5);
        if (xmin < 0) {
            xmin = 0;
        }
        int xmax = (int)(center + support + 0.5);
        if (xmax > inLen) {
            xmax = inLen;
        }
        std::vector<double> k(xmax - xmin);
        double total = 0.0;
        for (int i = xmin; i < xmax; i++) {
            k[i - xmin] = lanczos3((i - center + 0.5) / filterScale);
            total += k[i - xmin];
        }
        if (total != 0.0) {
            for (double& w : k) {
                w /= total;
            }
        }
        int lines = horizontal ? inH : inW;
        for (int l = 0; l < lines; l++) {
            double acc[4] = { 0, 0, 0, 0 };
            for (int i = xmin; i < xmax; i++) {
                size_t idx = horizontal ? ((size_t)l * inW + i) * 4 : ((size_t)i * inW + l) * 4;
                for (int c = 0; c < 4; c++) {
                    acc[c] += in[idx + c] * k[i - xmin];
                }
            }
            size_t oidx = horizontal ? ((size_t)l * outW + o) * 4 : ((size_t)o * outW + l) * 4;
            for (int c = 0; c < 4; c++) {
                // Pillow repasse en 8 bits entre les deux passes.
                double v = std::nearbyint(acc[c]);
                out[oidx + c] = v < 0.0 ? 0.0 : (v > 255.0 ? 255.0 : v);
            }
        }
    }
    return out;
}

// Réduction de moitié (Image.resize(LANCZOS) de Pillow, alpha prémultiplié).
static Image shrink(const Image& img, double scale = 0.5) {
    int ow = py_round(img.w * scale);
    int oh = py_round(img.h * scale);
    if (ow < 1) {
        ow = 1;
    }
    if (oh < 1) {
        oh = 1;
    }
    std::vector<double> buf((size_t)img.w * img.h * 4);
    for (size_t i = 0; i < (size_t)img.w * img.h; i++) {
        const uint8_t* p = &img.px[i * 4];
        double a = p[3];
        for (int c = 0; c < 3; c++) {
            buf[i * 4 + c] = std::nearbyint(p[c] * a / 255.0);
        }
        buf[i * 4 + 3] = a;
    }
    std::vector<double> h = resample_axis(buf, img.w, img.h, ow, true);
    std::vector<double> v = resample_axis(h, ow, img.h, oh, false);
    Image out(ow, oh);
    for (size_t i = 0; i < (size_t)ow * oh; i++) {
        double a = v[i * 4 + 3];
        for (int c = 0; c < 3; c++) {
            out.px[i * 4 + c] = (a > 0.0) ? clamp_u8(v[i * 4 + c] * 255.0 / a) : 0;
        }
        out.px[i * 4 + 3] = clamp_u8(a);
    }
    return out;
}

// Teinte les pixels clairs (le remplissage crème) dans `color`, garde le contour sombre.
static Image recolor_fill(const Image& img, int cr, int cg, int cb) {
    Image out = img;
    for (size_t i = 0; i < (size_t)img.w * img.h; i++) {
        const uint8_t* p = &img.px[i * 4];
        double lum = (p[0] + p[1] + p[2]) / 765.0;
        double t = (lum - 0.35) / 0.5;
        t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        if (t > 0.0) {
            double k = t * (0.75 + 0.25 * lum);
            out.px[i * 4 + 0] = clamp_u8(p[0] * (1.0 - t) + cr * k);
            out.px[i * 4 + 1] = clamp_u8(p[1] * (1.0 - t) + cg * k);
            out.px[i * 4 + 2] = clamp_u8(p[2] * (1.0 - t) + cb * k);
        }
    }
    return out;
}

// Contour : alpha dilaté (MaxFilter carré) de couleur `color`, l'image posée par-dessus.
static Image outline(const Image& img, int radius, int r, int g, int b) {
    Image out(img.w, img.h);
    for (int y = 0; y < img.h; y++) {
        for (int x = 0; x < img.w; x++) {
            int a = 0;
            for (int dy = -radius; dy <= radius; dy++) {
                int sy = y + dy < 0 ? 0 : (y + dy >= img.h ? img.h - 1 : y + dy);
                for (int dx = -radius; dx <= radius; dx++) {
                    int sx = x + dx < 0 ? 0 : (x + dx >= img.w ? img.w - 1 : x + dx);
                    int v = img.at(sx, sy)[3];
                    if (v > a) {
                        a = v;
                    }
                }
            }
            // alpha_composite(img par-dessus la base)
            const uint8_t* s = img.at(x, y);
            double sa = s[3] / 255.0, da = a / 255.0;
            double oa = sa + da * (1.0 - sa);
            uint8_t* o = out.at(x, y);
            if (oa <= 0.0) {
                continue;
            }
            const int base[3] = { r, g, b };
            for (int c = 0; c < 3; c++) {
                o[c] = clamp_u8((s[c] * sa + base[c] * da * (1.0 - sa)) / oa);
            }
            o[3] = clamp_u8(oa * 255.0);
        }
    }
    return out;
}

static Image darken(const Image& img, double f) {
    Image out = img;
    for (size_t i = 0; i < (size_t)img.w * img.h; i++) {
        for (int c = 0; c < 3; c++) {
            out.px[i * 4 + c] = clamp_u8(img.px[i * 4 + c] * f);
        }
    }
    return out;
}

static Image mirror(const Image& img) {
    Image out(img.w, img.h);
    for (int y = 0; y < img.h; y++) {
        for (int x = 0; x < img.w; x++) {
            std::memcpy(out.at(x, y), img.at(img.w - 1 - x, y), 4);
        }
    }
    return out;
}

// ------------------------------------------------------------------------------------------------
// Découpe des pièces (mêmes zones que tools/gen_assets.py, en pixels de l'atlas 4096x2048)
// ------------------------------------------------------------------------------------------------
static std::vector<Image> sPieces;

static void build_pieces(const Image& atlas) {
    sPieces.assign(TP_COUNT, Image());

    static const int bars[3][4] = { { 719, 450, 1201, 509 }, { 719, 512, 1201, 571 }, { 719, 575, 1201, 636 } };
    for (int i = 0; i < 3; i++) {
        Image b = shrink(crop(atlas, bars[i][0], bars[i][1], bars[i][2], bars[i][3]));
        sPieces[TP_DAY1_DIM + i] = darken(b, 0.45);
        sPieces[TP_DAY1 + i] = b;
    }

    static const int runs[CLOCK3DS_DIGIT_GLYPHS][2] = { { 5, 60 },    { 73, 110 },  { 133, 188 }, { 196, 251 },
                                                        { 257, 317 }, { 324, 380 }, { 388, 443 }, { 452, 507 },
                                                        { 516, 571 }, { 579, 634 }, { 649, 665 } };
    for (int n = 0; n < CLOCK3DS_DIGIT_GLYPHS; n++) {
        Image g = crop(atlas, runs[n][0] - 4, 584, runs[n][1] + 4, 692);
        sPieces[TP_DIGITS + 0 * CLOCK3DS_DIGIT_GLYPHS + n] = shrink(g);
        sPieces[TP_DIGITS + 1 * CLOCK3DS_DIGIT_GLYPHS + n] = shrink(recolor_fill(g, 90, 225, 255));
        sPieces[TP_DIGITS + 2 * CLOCK3DS_DIGIT_GLYPHS + n] = shrink(outline(recolor_fill(g, 255, 150, 20), 5, 20, 10, 0));
    }

    sPieces[TP_SUN] = shrink(crop(atlas, 578, 322, 672, 419));
    sPieces[TP_MOON] = shrink(crop(atlas, 589, 456, 668, 540));
    sPieces[TP_CURSOR] = shrink(crop(atlas, 1774, 577, 1815, 599));
    sPieces[TP_ARROW] = shrink(crop(atlas, 2755, 1948, 2891, 2043));

    Image swirl = shrink(crop(atlas, 2692, 19, 3132, 132));
    sPieces[TP_SWIRL_R] = mirror(swirl);
    sPieces[TP_SWIRL_L] = swirl;

    // Ornement central : centré sur la colonne où le motif monte le plus haut.
    Image strip = crop(atlas, 3062, 1837, 4096, 1912);
    int peak = 0, best = -1;
    for (int x = 0; x < strip.w; x++) {
        int top = strip.h;
        for (int y = 0; y < strip.h; y++) {
            if (strip.at(x, y)[3] > 60) {
                top = y;
                break;
            }
        }
        if (strip.h - top > best) {
            best = strip.h - top;
            peak = x;
        }
    }
    int x0 = peak - 150 < 0 ? 0 : peak - 150;
    int x1 = peak + 150 > strip.w ? strip.w : peak + 150;
    sPieces[TP_ORN] = shrink(crop(strip, x0, 0, x1, strip.h));
}

static bool read_file(const std::string& utf8Path, std::vector<uint8_t>& data) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, &wpath[0], wlen);
    FILE* f = _wfopen(wpath.c_str(), L"rb");
#else
    FILE* f = std::fopen(utf8Path.c_str(), "rb");
#endif
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    // L'atlas fait environ 2 Mo ; on refuse tout fichier démesuré.
    if (size <= 0 || size > 64L * 1024 * 1024) {
        std::fclose(f);
        return false;
    }
    data.resize((size_t)size);
    bool ok = std::fread(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    return ok;
}

static bool is_directory(const std::string& utf8Path) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, &wpath[0], wlen);
    DWORD attr = GetFileAttributesW(wpath.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(utf8Path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

// Nettoie le chemin saisi : espaces et guillemets autour (« Copier en tant que chemin » de Windows).
static std::string clean_path(std::string p) {
    auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' || c == '\''; };
    while (!p.empty() && blank(p.back())) {
        p.pop_back();
    }
    size_t start = 0;
    while (start < p.size() && blank(p[start])) {
        start++;
    }
    p = p.substr(start);
#ifndef _WIN32
    // "~/..." : dossier personnel (Linux, macOS).
    if (p.size() >= 2 && p[0] == '~' && p[1] == '/') {
        const char* home = std::getenv("HOME");
        if (home != nullptr && home[0] != '\0') {
            p = std::string(home) + p.substr(1);
        }
    }
#endif
    return p;
}

// Le chemin peut désigner le fichier lui-même ou un dossier du pack : on y cherche le fichier aux
// emplacements habituels (racine du pack, dossier user, load, textures, dossier du jeu, dossier UI).
static bool resolve_atlas(const std::string& input, std::string& out) {
    if (!is_directory(input)) {
        out = input;
        return true;
    }
    std::string dir = input;
    if (dir.back() != '/' && dir.back() != '\\') {
        dir += '/';
    }
    static const char* const subdirs[] = {
        "", "UI/", "0004000000125500/UI/", "textures/0004000000125500/UI/", "load/textures/0004000000125500/UI/",
        "user/load/textures/0004000000125500/UI/",
    };
    for (const char* sub : subdirs) {
        std::string candidate = dir + sub + CLOCK3DS_ATLAS_FILE;
        std::vector<uint8_t> probe;
#ifdef _WIN32
        int wlen = MultiByteToWideChar(CP_UTF8, 0, candidate.c_str(), -1, nullptr, 0);
        std::wstring wpath(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, candidate.c_str(), -1, &wpath[0], wlen);
        DWORD attr = GetFileAttributesW(wpath.c_str());
        bool exists = attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        bool exists = stat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
        if (exists) {
            out = candidate;
            return true;
        }
    }
    return false;
}

static bool load_atlas(const std::string& input) {
    std::string path;
    if (!resolve_atlas(input, path)) {
        std::printf("[3DS Clock] %s not found there: clock without textures.\n", CLOCK3DS_ATLAS_FILE);
        return false;
    }
    std::vector<uint8_t> file;
    if (!read_file(path, file)) {
        std::printf("[3DS Clock] Cannot read the texture file: clock without textures.\n");
        return false;
    }
    int w = 0, h = 0, comp = 0;
    if (!stbi_info_from_memory(file.data(), (int)file.size(), &w, &h, &comp) || w != CLOCK3DS_ATLAS_W ||
        h != CLOCK3DS_ATLAS_H) {
        std::printf("[3DS Clock] Not the expected atlas (%dx%d instead of %dx%d): clock without textures.\n",
                    w, h, CLOCK3DS_ATLAS_W, CLOCK3DS_ATLAS_H);
        return false;
    }
    uint8_t* pixels = stbi_load_from_memory(file.data(), (int)file.size(), &w, &h, &comp, 4);
    if (!pixels) {
        std::printf("[3DS Clock] Cannot decode the texture file: clock without textures.\n");
        return false;
    }
    Image atlas;
    atlas.w = w;
    atlas.h = h;
    atlas.px.assign(pixels, pixels + (size_t)w * h * 4);
    stbi_image_free(pixels);
    build_pieces(atlas);
    for (const Image& p : sPieces) {
        if (p.w <= 0 || p.h <= 0 || (size_t)p.w * p.h > CLOCK3DS_MAX_PIECE_PIXELS) {
            sPieces.clear();
            return false;
        }
    }
    std::printf("[3DS Clock] Textures loaded.\n");
    return true;
}

// ------------------------------------------------------------------------------------------------
// Fonctions appelées par le mod
// ------------------------------------------------------------------------------------------------

// u32 clock3ds_tex_load(const char* path) : découpe les pièces depuis le fichier (ou dossier) saisi dans
// les réglages du mod. Renvoie le nombre de pièces prêtes (TP_COUNT), ou 0 (chemin vide, fichier absent
// ou invalide). Chaque appel recommence depuis zéro.
CLOCK3DS_EXPORT void clock3ds_tex_load(uint8_t* rdram, void* ctx) {
    uint32_t addr = arg_u32(ctx, 0);
    std::vector<Image>().swap(sPieces);
    std::string input;
    if (addr >= 0x80000000u) {
        // Chaîne C dans la mémoire du jeu (octets permutés par mot de 32 bits), bornée.
        for (uint32_t i = 0; i < CLOCK3DS_PATH_MAX; i++) {
            char c = (char)rdram[((addr + i) ^ 3u) - 0x80000000u];
            if (c == '\0') {
                break;
            }
            input.push_back(c);
        }
    }
    input = clean_path(input);
    if (!input.empty()) {
        try {
            if (!load_atlas(input)) {
                sPieces.clear();
            }
        } catch (...) {
            sPieces.clear();
        }
    }
    ret_u32(ctx, sPieces.empty() ? 0 : (uint32_t)TP_COUNT);
}

// u32 clock3ds_tex_dims(u32 piece) : (largeur << 16) | hauteur, ou 0.
CLOCK3DS_EXPORT void clock3ds_tex_dims(uint8_t* rdram, void* ctx) {
    (void)rdram;
    uint32_t i = arg_u32(ctx, 0);
    if (i >= sPieces.size()) {
        ret_u32(ctx, 0);
        return;
    }
    ret_u32(ctx, ((uint32_t)sPieces[i].w << 16) | (uint32_t)sPieces[i].h);
}

// u32 clock3ds_tex_copy(u32 piece, void* dst, u32 dst_size) : copie les pixels RGBA32 de la pièce
// dans la mémoire du jeu (ordre des octets de la RDRAM recompilée). 1 si réussi.
CLOCK3DS_EXPORT void clock3ds_tex_copy(uint8_t* rdram, void* ctx) {
    uint32_t i = arg_u32(ctx, 0);
    uint32_t dst = arg_u32(ctx, 1);
    uint32_t dstSize = arg_u32(ctx, 2);
    if (i >= sPieces.size()) {
        ret_u32(ctx, 0);
        return;
    }
    const std::vector<uint8_t>& px = sPieces[i].px;
    if (dst < 0x80000000u || dstSize < px.size() || (uint64_t)dst + px.size() > 0x100000000ull) {
        ret_u32(ctx, 0);
        return;
    }
    for (size_t b = 0; b < px.size(); b++) {
        rdram[((dst + (uint32_t)b) ^ 3u) - 0x80000000u] = px[b];
    }
    ret_u32(ctx, 1);
}

// void clock3ds_tex_free(void) : libère les pièces une fois les textures créées.
CLOCK3DS_EXPORT void clock3ds_tex_free(uint8_t* rdram, void* ctx) {
    (void)rdram;
    (void)ctx;
    std::vector<Image>().swap(sPieces);
}

#ifdef CLOCK3DS_TEST
// Outil de test : écrit chaque pièce en .rgba pour la comparer à tools/gen_assets.py.
int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: %s <atlas or pack folder> <output folder>\n", argv[0]);
        return 1;
    }
    std::string path;
    if (!resolve_atlas(clean_path(argv[1]), path)) {
        std::printf("not found\n");
        return 2;
    }
    std::printf("%s\n", path.c_str());
    std::vector<uint8_t> file;
    if (!read_file(path, file)) {
        return 1;
    }
    int w, h, comp;
    uint8_t* pixels = stbi_load_from_memory(file.data(), (int)file.size(), &w, &h, &comp, 4);
    if (!pixels) {
        return 1;
    }
    Image atlas;
    atlas.w = w;
    atlas.h = h;
    atlas.px.assign(pixels, pixels + (size_t)w * h * 4);
    stbi_image_free(pixels);
    build_pieces(atlas);
    for (int i = 0; i < TP_COUNT; i++) {
        char name[512];
        std::snprintf(name, sizeof(name), "%s/%02d_%dx%d.rgba", argv[2], i, sPieces[i].w, sPieces[i].h);
        FILE* f = std::fopen(name, "wb");
        std::fwrite(sPieces[i].px.data(), 1, sPieces[i].px.size(), f);
        std::fclose(f);
    }
    return 0;
}
#endif
