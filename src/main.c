#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define GRID_COLS 5
#define GRID_ROWS 4
#define COMPOUND_COUNT 20

#define MAX_ATOMS 64
#define MAX_BONDS 96

#define PI 3.14159265f

typedef struct { float x, y, z; } Vec3;

typedef struct { int from, to; int order; } Bond;

typedef struct {
    const char *name;
    uint32_t colorRGBA;    
    int presetType;
    float baseScale;
} Compound;

typedef struct {
    Vec3 atomPos[MAX_ATOMS];
    uint8_t atomLabel[MAX_ATOMS]; // 0=C, 1=O, 2=N, 3=Cl, 4=F
    int atomCount;

    Bond bonds[MAX_BONDS];
    int bondCount;
} MoleculeGeometry;

typedef struct { int x, y, w, h; } RectI;

typedef struct {
    float yaw;
    float pitch;
    float panX;
    float panY;
    float zoomMultiplier;
} ViewControl;

typedef struct {
    int atomIndex;
    float depth;
    int screenX;
    int screenY;
    int radius;
    uint32_t color;
} AtomDraw;

typedef struct {
    int x1,y1,x2,y2;
    float depth;
    int order;
    uint32_t color;
    uint8_t alpha;
} BondDraw;


static inline uint8_t color_r(uint32_t c){ return (c >> 24) & 255; }
static inline uint8_t color_g(uint32_t c){ return (c >> 16) & 255; }
static inline uint8_t color_b(uint32_t c){ return (c >>  8) & 255; }

static float clampf(float v, float lo, float hi){
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static Vec3 make_vec3(float x, float y, float z){
    Vec3 v = {x,y,z};
    return v;
}

static void set_draw_color(SDL_Renderer *renderer, uint32_t rgba, uint8_t a){
    SDL_SetRenderDrawColor(renderer, color_r(rgba), color_g(rgba), color_b(rgba), a);
}

static uint32_t lighten(uint32_t rgba, float t){
    uint8_t r = color_r(rgba);
    uint8_t g = color_g(rgba);
    uint8_t b = color_b(rgba);

    uint8_t nr = (uint8_t)clampf(r + (255 - r)*t, 0, 255);
    uint8_t ng = (uint8_t)clampf(g + (255 - g)*t, 0, 255);
    uint8_t nb = (uint8_t)clampf(b + (255 - b)*t, 0, 255);
    return ((uint32_t)nr<<24) | ((uint32_t)ng<<16) | ((uint32_t)nb<<8) | 0xFF;
}

static uint32_t darken(uint32_t rgba, float t){
    uint8_t r = color_r(rgba);
    uint8_t g = color_g(rgba);
    uint8_t b = color_b(rgba);

    uint8_t nr = (uint8_t)clampf(r*(1.0f - t), 0, 255);
    uint8_t ng = (uint8_t)clampf(g*(1.0f - t), 0, 255);
    uint8_t nb = (uint8_t)clampf(b*(1.0f - t), 0, 255);
    return ((uint32_t)nr<<24) | ((uint32_t)ng<<16) | ((uint32_t)nb<<8) | 0xFF;
}


static Vec3 rotate_yaw_pitch(Vec3 p, float yaw, float pitch){
    float cy = cosf(yaw), sy = sinf(yaw);
    float x1 =  cy * p.x + sy * p.z;
    float z1 = -sy * p.x + cy * p.z;

    float cp = cosf(pitch), sp = sinf(pitch);
    float y2 =  cp * p.y - sp * z1;
    float z2 =  sp * p.y + cp * z1;

    return make_vec3(x1, y2, z2);
}

static void project_to_screen(Vec3 p, float zoom, int centerX, int centerY, int *outX, int *outY, float *outDepth){
    float perspective = 900.0f / (900.0f + p.z);
    *outX = (int)lroundf(centerX + p.x * zoom * perspective);
    *outY = (int)lroundf(centerY - p.y * zoom * perspective);
    *outDepth = p.z;
}


static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius){
    for(int y = -radius; y <= radius; y++){
        for(int x = -radius; x <= radius; x++){
            if(x*x + y*y <= radius*radius){
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            }
        }
    }
}

static void draw_thick_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int thickness){
    if(thickness < 2) thickness = 2;

    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);
    float len = sqrtf(dx*dx + dy*dy);
    if(len < 1e-4f) return;

    float normalX = -dy / len;
    float normalY =  dx / len;

    int steps = (int)len;
    float half = thickness * 0.5f;

    for(int i = 0; i <= steps; i++){
        float t = (steps == 0) ? 0.0f : (float)i / (float)steps;
        int x = (int)lroundf((float)x1 + dx * t);
        int y = (int)lroundf((float)y1 + dy * t);

        for(int j = -(int)half; j <= (int)half; j++){
            int px = x + (int)lroundf(normalX * (float)j);
            int py = y + (int)lroundf(normalY * (float)j);
            SDL_RenderDrawPoint(renderer, px, py);
        }
    }
}

static void draw_stick(SDL_Renderer *renderer, int x1,int y1,int x2,int y2, int order, uint32_t baseColor, uint8_t alpha){
    int outer = (order == 1) ? 6 : (order == 2 ? 8 : 10);
    int inner = (order == 1) ? 3 : (order == 2 ? 4 : 5);

    uint32_t dark = darken(baseColor, 0.35f);
    uint32_t bright = lighten(baseColor, 0.35f);

    set_draw_color(renderer, dark, alpha);
    draw_thick_line(renderer, x1,y1,x2,y2, outer);

    set_draw_color(renderer, bright, alpha);
    draw_thick_line(renderer, x1,y1,x2,y2, inner);

    if(order == 2){
        set_draw_color(renderer, dark, alpha);
        draw_thick_line(renderer, x1+3,y1-3,x2+3,y2-3, outer-2);
        set_draw_color(renderer, bright, alpha);
        draw_thick_line(renderer, x1+3,y1-3,x2+3,y2-3, inner-1);
    }
}


static void draw_glyph3x5(SDL_Renderer *renderer, int x, int y, int scale, const uint8_t bits[5]){
    for(int row = 0; row < 5; row++){
        for(int col = 0; col < 3; col++){
            if(bits[row] & (1 << (2 - col))){
                SDL_Rect px = { x + col * scale, y + row * scale, scale, scale };
                SDL_RenderFillRect(renderer, &px);
            }
        }
    }
}

static const uint8_t GL_C[5] = {0b111,0b100,0b100,0b100,0b111};
static const uint8_t GL_O[5] = {0b111,0b101,0b101,0b101,0b111};
static const uint8_t GL_N[5] = {0b101,0b111,0b111,0b111,0b101};
static const uint8_t GL_F[5] = {0b111,0b100,0b110,0b100,0b100};
static const uint8_t GL_L[5] = {0b100,0b100,0b100,0b100,0b111};

static void draw_atom_label(SDL_Renderer *renderer, int x, int y, int labelCode){
    int scale = 2;
    if(labelCode == 0) draw_glyph3x5(renderer, x, y, scale, GL_C);
    else if(labelCode == 1) draw_glyph3x5(renderer, x, y, scale, GL_O);
    else if(labelCode == 2) draw_glyph3x5(renderer, x, y, scale, GL_N);
    else if(labelCode == 4) draw_glyph3x5(renderer, x, y, scale, GL_F);
    else if(labelCode == 3){
        draw_glyph3x5(renderer, x, y, scale, GL_C);
        draw_glyph3x5(renderer, x + 8, y, scale, GL_L);
    }
}


static void add_atom(MoleculeGeometry *mol, Vec3 p, uint8_t label){
    if(mol->atomCount >= MAX_ATOMS) return;
    mol->atomPos[mol->atomCount] = p;
    mol->atomLabel[mol->atomCount] = label;
    mol->atomCount++;
}

static void add_bond(MoleculeGeometry *mol, int a, int b, int order){
    if(mol->bondCount >= MAX_BONDS) return;
    mol->bonds[mol->bondCount++] = (Bond){a,b,order};
}

static void build_steroid_core(MoleculeGeometry *mol){
    mol->atomCount = 0;
    mol->bondCount = 0;

    int baseA = mol->atomCount;
    for(int i = 0; i < 6; i++){
        float ang = i * PI / 3.0f;
        add_atom(mol, make_vec3(cosf(ang) * 1.8f, sinf(ang) * 1.8f, 0.0f), 0);
    }
    for(int i = 0; i < 6; i++) add_bond(mol, baseA+i, baseA+((i+1)%6), 1);

    int baseB = mol->atomCount;
    for(int i = 0; i < 6; i++){
        float ang = i * PI / 3.0f;
        add_atom(mol, make_vec3(2.8f + cosf(ang) * 1.8f, sinf(ang) * 1.8f, 0.2f), 0);
    }
    for(int i = 0; i < 6; i++) add_bond(mol, baseB+i, baseB+((i+1)%6), 1);
    add_bond(mol, baseA+1, baseB+4, 1);
    add_bond(mol, baseA+2, baseB+5, 1);

    int baseC = mol->atomCount;
    for(int i = 0; i < 5; i++){
        float ang = i * 2.0f * PI / 5.0f;
        add_atom(mol, make_vec3(2.8f + cosf(ang) * 1.6f, 2.8f + sinf(ang) * 1.6f, -0.2f), 0);
    }
    for(int i = 0; i < 5; i++) add_bond(mol, baseC+i, baseC+((i+1)%5), 1);
    add_bond(mol, baseB+1, baseC+3, 1);
    add_bond(mol, baseB+2, baseC+4, 1);

    int baseD = mol->atomCount;
    for(int i = 0; i < 5; i++){
        float ang = i * 2.0f * PI / 5.0f;
        add_atom(mol, make_vec3(4.8f + cosf(ang) * 1.3f, 4.3f + sinf(ang) * 1.3f, 0.15f), 0);
    }
    for(int i = 0; i < 5; i++) add_bond(mol, baseD+i, baseD+((i+1)%5), 1);
    add_bond(mol, baseC+1, baseD+3, 1);
    add_bond(mol, baseC+2, baseD+4, 1);
}

static void add_ester_tail(MoleculeGeometry *mol, int attachAtom, int segmentCount, float zWiggle){
    int previous = attachAtom;
    for(int i = 0; i < segmentCount; i++){
        Vec3 base = mol->atomPos[attachAtom];
        float step = 1.1f;
        Vec3 next = make_vec3(base.x + (i+1)*step, base.y - 0.4f*(float)i, base.z + zWiggle*(float)i);
        add_atom(mol, next, 0);
        int current = mol->atomCount - 1;
        add_bond(mol, previous, current, 1);
        previous = current;
    }
}

static void apply_preset(MoleculeGeometry *mol, int presetType){
    build_steroid_core(mol);

    int attachHydroxyl = 2;
    int attachCarbonyl = 8;
    int attachEster = 14;

    add_atom(mol, make_vec3(mol->atomPos[attachHydroxyl].x - 0.2f,
                            mol->atomPos[attachHydroxyl].y + 1.4f,
                            mol->atomPos[attachHydroxyl].z + 0.8f), 1);
    add_bond(mol, attachHydroxyl, mol->atomCount-1, 1);

    add_atom(mol, make_vec3(mol->atomPos[attachCarbonyl].x + 0.3f,
                            mol->atomPos[attachCarbonyl].y - 1.2f,
                            mol->atomPos[attachCarbonyl].z - 0.6f), 1);
    add_bond(mol, attachCarbonyl, mol->atomCount-1, 2);

    if(presetType == 1) add_ester_tail(mol, attachEster, 6, 0.10f);
    if(presetType == 2) add_ester_tail(mol, attachEster, 7, -0.05f);
    if(presetType == 18) add_ester_tail(mol, attachEster, 3, 0.05f);
    if(presetType == 19) add_ester_tail(mol, attachEster, 3, -0.06f);

    if(presetType == 6 || presetType == 16 || presetType == 17){
        add_atom(mol, make_vec3(mol->atomPos[5].x - 1.2f, mol->atomPos[5].y + 0.6f, mol->atomPos[5].z + 0.2f), 0);
        add_bond(mol, 5, mol->atomCount-1, 1);
    }

    if(presetType == 3 || presetType == 5){
        if(mol->bondCount > 10){
            mol->bonds[3].order = 2;
            mol->bonds[8].order = 2;
        }
        if(presetType == 3 && mol->bondCount > 15){
            mol->bonds[12].order = 2;
        }
    }

    if(presetType == 8){
        add_atom(mol, make_vec3(mol->atomPos[0].x - 1.6f, mol->atomPos[0].y + 0.2f, mol->atomPos[0].z), 2);
        add_bond(mol, 0, mol->atomCount-1, 1);
    }

    if(presetType == 12 || presetType == 17){
        add_atom(mol, make_vec3(mol->atomPos[1].x - 1.2f, mol->atomPos[1].y + 1.0f, mol->atomPos[1].z + 0.1f), 3);
        add_bond(mol, 1, mol->atomCount-1, 1);
    }

    if(presetType == 13){
        add_atom(mol, make_vec3(mol->atomPos[9].x + 1.1f, mol->atomPos[9].y + 0.8f, mol->atomPos[9].z - 0.2f), 4);
        add_bond(mol, 9, mol->atomCount-1, 1);
    }

    if(presetType == 7 || presetType == 9){
        add_atom(mol, make_vec3(mol->atomPos[10].x + 0.2f, mol->atomPos[10].y + 1.1f, mol->atomPos[10].z - 0.4f), 1);
        add_bond(mol, 10, mol->atomCount-1, 1);
    }

    if(presetType == 4 || presetType == 10 || presetType == 14 || presetType == 15){
        add_atom(mol, make_vec3(mol->atomPos[11].x + 0.9f, mol->atomPos[11].y - 0.9f, mol->atomPos[11].z + 0.3f), 0);
        add_bond(mol, 11, mol->atomCount-1, 1);
    }

    if(presetType == 11){
        add_atom(mol, make_vec3(mol->atomPos[6].x + 0.7f, mol->atomPos[6].y + 1.0f, mol->atomPos[6].z), 0);
        add_bond(mol, 6, mol->atomCount-1, 1);
    }
}

static float compute_bounding_radius(const MoleculeGeometry *mol){
    float maxR2 = 0.0f;
    for(int i = 0; i < mol->atomCount; i++){
        float x = mol->atomPos[i].x;
        float y = mol->atomPos[i].y;
        float z = mol->atomPos[i].z;
        float r2 = x*x + y*y + z*z;
        if(r2 > maxR2) maxR2 = r2;
    }
    float r = sqrtf(maxR2);
    if(r < 0.001f) r = 1.0f;
    return r;
}

static void reset_view_control(ViewControl *v){
    v->yaw = 0.0f;
    v->pitch = 0.5f;
    v->panX = 0.0f;
    v->panY = 0.0f;
    v->zoomMultiplier = 1.0f;
}


static const Compound compounds[COMPOUND_COUNT] = {
    {"Testosterone",                 0xE74C3CFF, 0,  1.00f},
    {"Testosterone Enanthate",       0x00D2D3FF, 1,  1.00f},
    {"Testosterone Cypionate",       0xF1C40FFF, 2,  1.00f},
    {"Trenbolone",                   0xE056FDFF, 3,  1.00f},
    {"Nandrolone / Deca",            0x2ECC71FF, 4,  1.00f},
    {"Boldenone / EQ",               0xE67E22FF, 5,  1.00f},
    {"Dianabol",                     0x3498DBFF, 6,  1.00f},
    {"Anavar (Oxandrolone)",         0xFF6B81FF, 7,  1.00f},
    {"Winstrol (Stanozolol)",        0xF1C40FFF, 8,  1.00f},
    {"Anadrol (Oxymetholone)",       0x8E1B1BFF, 9,  1.00f},
    {"Masteron (Drostanolone)",      0x7BED9FFF,10,  1.00f},
    {"Primobolan (Methenolone)",     0x9B59B6FF,11,  1.00f},
    {"Turinabol",                    0x00D2D3FF,12,  1.00f},
    {"Halotestin (Fluoxymesterone)", 0xE67E22FF,13,  1.00f},
    {"Proviron (Mesterolone)",       0x6C5CE7FF,14,  1.00f},
    {"Mibolerone",                   0xA55EEAFF,15,  1.00f},
    {"Superdrol",                    0x8E44ADFF,16,  1.00f},
    {"Oral Turinabol",               0x1ABC9CFF,17,  1.00f},
    {"Testosterone Propionate",      0xFF6B81FF,18,  1.00f},
    {"NPP",                          0x00CEC9FF,19,  1.00f},
};


static RectI get_tile_rect(int index){
    int col = index % GRID_COLS;
    int row = index / GRID_COLS;

    int padding = 14;
    int tileW = (WINDOW_WIDTH - padding * (GRID_COLS + 1)) / GRID_COLS;
    int tileH = (WINDOW_HEIGHT - padding * (GRID_ROWS + 1)) / GRID_ROWS;

    int x = padding + col * (tileW + padding);
    int y = padding + row * (tileH + padding);
    return (RectI){x, y, tileW, tileH};
}


static int sort_bonds_far_to_near(const void *a, const void *b){
    const BondDraw *A = (const BondDraw*)a;
    const BondDraw *B = (const BondDraw*)b;
    if(A->depth < B->depth) return -1;
    if(A->depth > B->depth) return  1;
    return 0;
}

static int sort_atoms_far_to_near(const void *a, const void *b){
    const AtomDraw *A = (const AtomDraw*)a;
    const AtomDraw *B = (const AtomDraw*)b;
    if(A->depth < B->depth) return -1;
    if(A->depth > B->depth) return  1;
    return 0;
}


static void draw_molecule(SDL_Renderer *renderer,
                          const Compound *compound,
                          const MoleculeGeometry *mol,
                          const RectI *rect,
                          bool isSelected,
                          bool isWireframe,
                          float timeSeconds,
                          const ViewControl *view,
                          bool autoRotateEnabled){
    SDL_Rect clip = { rect->x, rect->y, rect->w, rect->h };
    SDL_RenderSetClipRect(renderer, &clip);

    SDL_SetRenderDrawColor(renderer, 16,16,20,255);
    SDL_RenderFillRect(renderer, &clip);

    if(isSelected) SDL_SetRenderDrawColor(renderer, 240,240,255,255);
    else SDL_SetRenderDrawColor(renderer, 60,60,75,255);
    SDL_RenderDrawRect(renderer, &clip);

    int centerX = rect->x + rect->w / 2;
    int centerY = rect->y + rect->h / 2;

    float radius = compute_bounding_radius(mol);
    float minDim = (float)((rect->w < rect->h) ? rect->w : rect->h);
    float baseZoom = (minDim * 0.38f) / radius;
    float zoom = baseZoom * compound->baseScale * view->zoomMultiplier;

    float autoYaw = 0.0f;
    float autoPitch = 0.0f;
    if(autoRotateEnabled){
        autoYaw = timeSeconds * 0.7f;
        autoPitch = timeSeconds * 0.25f;
    }

    float yaw = autoYaw + view->yaw;
    float pitch = autoPitch + view->pitch;

    int panX = (int)lroundf(view->panX);
    int panY = (int)lroundf(view->panY);

    BondDraw bondDraws[MAX_BONDS];
    int bondDrawCount = 0;

    AtomDraw atomDraws[MAX_ATOMS];
    int atomDrawCount = 0;

    int projectedX[MAX_ATOMS];
    int projectedY[MAX_ATOMS];
    float projectedDepth[MAX_ATOMS];

    for(int i = 0; i < mol->atomCount; i++){
        Vec3 rp = rotate_yaw_pitch(mol->atomPos[i], yaw, pitch);
        int sx, sy;
        float d;
        project_to_screen(rp, zoom, centerX + panX, centerY + panY, &sx, &sy, &d);
        projectedX[i] = sx;
        projectedY[i] = sy;
        projectedDepth[i] = d;
    }

    for(int i = 0; i < mol->bondCount; i++){
        Bond b = mol->bonds[i];

        int x1 = projectedX[b.from];
        int y1 = projectedY[b.from];
        int x2 = projectedX[b.to];
        int y2 = projectedY[b.to];

        float depth = (projectedDepth[b.from] + projectedDepth[b.to]) * 0.5f;

        uint8_t alpha = isSelected ? 230 : 140;

        bondDraws[bondDrawCount++] = (BondDraw){
            x1,y1,x2,y2,
            depth,
            b.order,
            compound->colorRGBA,
            alpha
        };
    }

    for(int i = 0; i < mol->atomCount; i++){
        uint32_t atomColor = compound->colorRGBA;
        if(mol->atomLabel[i] == 1) atomColor = 0xFF4757FF; // O
        if(mol->atomLabel[i] == 2) atomColor = 0x5F27CDFF; // N
        if(mol->atomLabel[i] == 3) atomColor = 0x1DD1A1FF; // Cl
        if(mol->atomLabel[i] == 4) atomColor = 0x48DBFBFF; // F

        float depthScale = clampf(1.0f - projectedDepth[i] * 0.05f, 0.6f, 1.35f);

        float baseSize = isWireframe ? 3.5f : 7.0f;
        if(mol->atomLabel[i] == 1) baseSize = isWireframe ? 4.0f : 8.0f;
        if(mol->atomLabel[i] == 3 || mol->atomLabel[i] == 4) baseSize = isWireframe ? 4.2f : 9.0f;

        int rad = (int)lroundf(baseSize * depthScale);

        atomDraws[atomDrawCount++] = (AtomDraw){
            i,
            projectedDepth[i],
            projectedX[i],
            projectedY[i],
            rad,
            atomColor
        };
    }

    qsort(bondDraws, bondDrawCount, sizeof(BondDraw), sort_bonds_far_to_near);
    qsort(atomDraws, atomDrawCount, sizeof(AtomDraw), sort_atoms_far_to_near);

    for(int i = 0; i < bondDrawCount; i++){
        BondDraw bd = bondDraws[i];

        if(isWireframe){
            uint32_t bright = lighten(bd.color, 0.20f);
            set_draw_color(renderer, darken(bd.color, 0.55f), bd.alpha);
            draw_thick_line(renderer, bd.x1,bd.y1,bd.x2,bd.y2, 4);

            set_draw_color(renderer, bright, bd.alpha);
            draw_thick_line(renderer, bd.x1,bd.y1,bd.x2,bd.y2, 2);

            if(bd.order == 2){
                set_draw_color(renderer, bright, bd.alpha);
                draw_thick_line(renderer, bd.x1+2,bd.y1-2,bd.x2+2,bd.y2-2, 2);
            }
        } else {
            draw_stick(renderer, bd.x1,bd.y1,bd.x2,bd.y2, bd.order, bd.color, bd.alpha);
        }
    }

    for(int i = 0; i < atomDrawCount; i++){
        AtomDraw ad = atomDraws[i];

        if(isWireframe){
            uint32_t c = lighten(ad.color, 0.05f);
            set_draw_color(renderer, c, isSelected ? 220 : 170);
            draw_filled_circle(renderer, ad.screenX, ad.screenY, (int)clampf(ad.radius, 2, 5));
            continue;
        }

        set_draw_color(renderer, darken(ad.color, 0.40f), 255);
        draw_filled_circle(renderer, ad.screenX, ad.screenY, ad.radius + 2);

        set_draw_color(renderer, ad.color, 255);
        draw_filled_circle(renderer, ad.screenX, ad.screenY, ad.radius);

        int hx = ad.screenX - ad.radius/3;
        int hy = ad.screenY - ad.radius/3;
        uint32_t hi = lighten(ad.color, 0.60f);
        set_draw_color(renderer, hi, 220);
        draw_filled_circle(renderer, hx, hy, (int)clampf(ad.radius*0.45f, 2, 12));

        set_draw_color(renderer, 0xFFFFFFFF, 200);
        draw_filled_circle(renderer, hx - 2, hy - 2, (int)clampf(ad.radius*0.12f, 1, 4));

        int label = mol->atomLabel[ad.atomIndex];
        if(isSelected && label != 0){
            SDL_SetRenderDrawColor(renderer, 245,245,255,255);
            draw_atom_label(renderer, ad.screenX + ad.radius + 4, ad.screenY - 6, label);
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);
}


int main(void){
    if(SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    SDL_Window *window = SDL_CreateWindow(
        "pk_rk4 | Structural Atlas",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if(!window) return 1;

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(!renderer) return 1;

    MoleculeGeometry moleculeCache[COMPOUND_COUNT];
    for(int i = 0; i < COMPOUND_COUNT; i++){
        apply_preset(&moleculeCache[i], compounds[i].presetType);
    }

    ViewControl viewControls[COMPOUND_COUNT];
    for(int i = 0; i < COMPOUND_COUNT; i++) reset_view_control(&viewControls[i]);

    int selectedIndex = 0;
    bool isWireframe = false;
    bool isFocused = false;
    bool autoRotateEnabled = true;

    bool leftDragging = false;
    bool rightDragging = false;
    int lastMouseX = 0;
    int lastMouseY = 0;

    uint32_t startTicks = SDL_GetTicks();
    bool running = true;

    while(running){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type == SDL_QUIT) running = false;

            if(e.type == SDL_KEYDOWN){
                SDL_Keycode key = e.key.keysym.sym;

                if(key == SDLK_ESCAPE) running = false;
                if(key == SDLK_SPACE) isWireframe = !isWireframe;
                if(key == SDLK_RETURN) isFocused = !isFocused;
                if(key == SDLK_r) reset_view_control(&viewControls[selectedIndex]);
                if(key == SDLK_a) autoRotateEnabled = !autoRotateEnabled;

                if(!isFocused){
                    if(key == SDLK_LEFT)  selectedIndex = (selectedIndex % GRID_COLS == 0) ? selectedIndex : selectedIndex - 1;
                    if(key == SDLK_RIGHT) selectedIndex = (selectedIndex % GRID_COLS == GRID_COLS - 1) ? selectedIndex : selectedIndex + 1;
                    if(key == SDLK_UP)    selectedIndex = (selectedIndex - GRID_COLS >= 0) ? selectedIndex - GRID_COLS : selectedIndex;
                    if(key == SDLK_DOWN)  selectedIndex = (selectedIndex + GRID_COLS < COMPOUND_COUNT) ? selectedIndex + GRID_COLS : selectedIndex;
                }
            }

            if(e.type == SDL_MOUSEBUTTONDOWN){
                if(e.button.button == SDL_BUTTON_LEFT){
                    leftDragging = true;
                    lastMouseX = e.button.x;
                    lastMouseY = e.button.y;
                }
                if(e.button.button == SDL_BUTTON_RIGHT){
                    rightDragging = true;
                    lastMouseX = e.button.x;
                    lastMouseY = e.button.y;
                }
            }

            if(e.type == SDL_MOUSEBUTTONUP){
                if(e.button.button == SDL_BUTTON_LEFT) leftDragging = false;
                if(e.button.button == SDL_BUTTON_RIGHT) rightDragging = false;
            }

            if(e.type == SDL_MOUSEMOTION){
                int mx = e.motion.x;
                int my = e.motion.y;
                int dx = mx - lastMouseX;
                int dy = my - lastMouseY;
                lastMouseX = mx;
                lastMouseY = my;

                ViewControl *v = &viewControls[selectedIndex];

                if(leftDragging){
                    v->yaw += (float)dx * 0.01f;
                    v->pitch += (float)dy * 0.01f;
                    v->pitch = clampf(v->pitch, -1.2f, 1.2f);
                }
                if(rightDragging){
                    v->panX += (float)dx;
                    v->panY += (float)dy;
                }
            }

            if(e.type == SDL_MOUSEWHEEL){
                ViewControl *v = &viewControls[selectedIndex];
                if(e.wheel.y > 0) v->zoomMultiplier *= 1.08f;
                if(e.wheel.y < 0) v->zoomMultiplier *= 0.92f;
                v->zoomMultiplier = clampf(v->zoomMultiplier, 0.35f, 4.0f);
            }
        }

        float timeSeconds = (SDL_GetTicks() - startTicks) * 0.001f;

        SDL_SetRenderDrawColor(renderer, 10,10,14,255);
        SDL_RenderClear(renderer);

        if(!isFocused){
            for(int i = 0; i < COMPOUND_COUNT; i++){
                RectI tile = get_tile_rect(i);
                bool tileSelected = (i == selectedIndex);

                draw_molecule(renderer,
                              &compounds[i],
                              &moleculeCache[i],
                              &tile,
                              tileSelected,
                              isWireframe,
                              timeSeconds,
                              &viewControls[i],
                              autoRotateEnabled);
            }

            char title[320];
            snprintf(title, sizeof(title),
                     "pk_rk4 | Structural Atlas | selected: %s | Space: mode | Enter: focus | Arrows: move | Mouse: rotate/pan/zoom | R: reset | A: auto %s",
                     compounds[selectedIndex].name,
                     autoRotateEnabled ? "ON" : "OFF");
            SDL_SetWindowTitle(window, title);
        } else {
            RectI focusRect = { 20, 20, WINDOW_WIDTH - 40, WINDOW_HEIGHT - 40 };

            draw_molecule(renderer,
                          &compounds[selectedIndex],
                          &moleculeCache[selectedIndex],
                          &focusRect,
                          true,
                          isWireframe,
                          timeSeconds,
                          &viewControls[selectedIndex],
                          autoRotateEnabled);

            char title[320];
            snprintf(title, sizeof(title),
                     "pk_rk4 | Focus: %s | Space: mode | Enter: back | Mouse: rotate/pan/zoom | R: reset | A: auto %s",
                     compounds[selectedIndex].name,
                     autoRotateEnabled ? "ON" : "OFF");
            SDL_SetWindowTitle(window, title);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
