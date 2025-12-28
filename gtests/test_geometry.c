#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_ATOMS 64
#define MAX_BONDS 96
#define PI 3.14159265f

typedef struct { float x,y,z; } Vec3;
typedef struct { int from, to; int order; } Bond;

typedef struct {
    Vec3 atomPos[MAX_ATOMS];
    uint8_t atomLabel[MAX_ATOMS];
    int atomCount;
    Bond bonds[MAX_BONDS];
    int bondCount;
} MoleculeGeometry;

static int tests_run = 0;
static int tests_failed = 0;

static void assert_true(bool cond, const char *msg){
    tests_run++;
    if(!cond){
        tests_failed++;
        printf("[FAIL] %s\n", msg);
    }
}

static bool is_finite_vec3(Vec3 v){
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

static Vec3 make_vec3(float x,float y,float z){ Vec3 v={x,y,z}; return v; }

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
    for(int i=0;i<6;i++){
        float ang = i*PI/3.0f;
        add_atom(mol, make_vec3(cosf(ang)*1.8f, sinf(ang)*1.8f, 0.0f), 0);
    }
    for(int i=0;i<6;i++) add_bond(mol, baseA+i, baseA+((i+1)%6), 1);

    int baseB = mol->atomCount;
    for(int i=0;i<6;i++){
        float ang = i*PI/3.0f;
        add_atom(mol, make_vec3(2.8f + cosf(ang)*1.8f, sinf(ang)*1.8f, 0.2f), 0);
    }
    for(int i=0;i<6;i++) add_bond(mol, baseB+i, baseB+((i+1)%6), 1);
    add_bond(mol, baseA+1, baseB+4, 1);
    add_bond(mol, baseA+2, baseB+5, 1);

    int baseC = mol->atomCount;
    for(int i=0;i<5;i++){
        float ang = i*2.0f*PI/5.0f;
        add_atom(mol, make_vec3(2.8f + cosf(ang)*1.6f, 2.8f + sinf(ang)*1.6f, -0.2f), 0);
    }
    for(int i=0;i<5;i++) add_bond(mol, baseC+i, baseC+((i+1)%5), 1);
    add_bond(mol, baseB+1, baseC+3, 1);
    add_bond(mol, baseB+2, baseC+4, 1);

    int baseD = mol->atomCount;
    for(int i=0;i<5;i++){
        float ang = i*2.0f*PI/5.0f;
        add_atom(mol, make_vec3(4.8f + cosf(ang)*1.3f, 4.3f + sinf(ang)*1.3f, 0.15f), 0);
    }
    for(int i=0;i<5;i++) add_bond(mol, baseD+i, baseD+((i+1)%5), 1);
    add_bond(mol, baseC+1, baseD+3, 1);
    add_bond(mol, baseC+2, baseD+4, 1);
}

static void add_ester_tail(MoleculeGeometry *mol, int attachAtom, int segmentCount, float zWiggle){
    int previous = attachAtom;
    for(int i=0;i<segmentCount;i++){
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
}

static void validate_molecule(const MoleculeGeometry *mol, const char *name){
    assert_true(mol->atomCount > 0, "atomCount > 0");
    assert_true(mol->bondCount > 0, "bondCount > 0");
    assert_true(mol->atomCount <= MAX_ATOMS, "atomCount within MAX_ATOMS");
    assert_true(mol->bondCount <= MAX_BONDS, "bondCount within MAX_BONDS");

    for(int i=0;i<mol->atomCount;i++){
        char msg[128];
        snprintf(msg, sizeof(msg), "%s atom %d finite", name, i);
        assert_true(is_finite_vec3(mol->atomPos[i]), msg);

        float ax = fabsf(mol->atomPos[i].x);
        float ay = fabsf(mol->atomPos[i].y);
        float az = fabsf(mol->atomPos[i].z);
        snprintf(msg, sizeof(msg), "%s atom %d reasonable magnitude", name, i);
        assert_true(ax < 200.0f && ay < 200.0f && az < 200.0f, msg);
    }

    for(int i=0;i<mol->bondCount;i++){
        Bond b = mol->bonds[i];
        char msg[128];

        snprintf(msg, sizeof(msg), "%s bond %d indices in range", name, i);
        assert_true(b.from >= 0 && b.from < mol->atomCount && b.to >= 0 && b.to < mol->atomCount, msg);

        snprintf(msg, sizeof(msg), "%s bond %d order valid", name, i);
        assert_true(b.order >= 1 && b.order <= 3, msg);

        snprintf(msg, sizeof(msg), "%s bond %d not self", name, i);
        assert_true(b.from != b.to, msg);
    }
}

int main(void){
    const int presetTypes[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};

    for(int i=0;i<20;i++){
        MoleculeGeometry mol;
        apply_preset(&mol, presetTypes[i]);

        char name[32];
        snprintf(name, sizeof(name), "preset_%d", presetTypes[i]);
        validate_molecule(&mol, name);
    }

    if(tests_failed == 0){
        printf("[OK] %d tests passed\n", tests_run);
        return 0;
    }
    printf("[FAIL] %d/%d tests failed\n", tests_failed, tests_run);
    return 1;
}
