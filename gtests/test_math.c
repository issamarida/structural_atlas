#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct { float x, y, z; } Vec3;

static float clampf(float v, float lo, float hi){
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static Vec3 make_vec3(float x, float y, float z){
    Vec3 v = {x,y,z};
    return v;
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

static int tests_run = 0;
static int tests_failed = 0;

static void assert_true(bool cond, const char *msg){
    tests_run++;
    if(!cond){
        tests_failed++;
        printf("[FAIL] %s\n", msg);
    }
}

static void assert_near(float a, float b, float eps, const char *msg){
    tests_run++;
    if(fabsf(a - b) > eps){
        tests_failed++;
        printf("[FAIL] %s (got %.6f expected %.6f)\n", msg, a, b);
    }
}

static void test_clampf(void){
    assert_near(clampf(5.0f, 0.0f, 10.0f), 5.0f, 1e-6f, "clampf inside range");
    assert_near(clampf(-2.0f, 0.0f, 10.0f), 0.0f, 1e-6f, "clampf low clamp");
    assert_near(clampf(99.0f, 0.0f, 10.0f), 10.0f, 1e-6f, "clampf high clamp");
}

static void test_rotate_identity(void){
    Vec3 p = make_vec3(1.0f, 2.0f, 3.0f);
    Vec3 q = rotate_yaw_pitch(p, 0.0f, 0.0f);
    assert_near(q.x, p.x, 1e-6f, "rotate yaw=0 pitch=0 x unchanged");
    assert_near(q.y, p.y, 1e-6f, "rotate yaw=0 pitch=0 y unchanged");
    assert_near(q.z, p.z, 1e-6f, "rotate yaw=0 pitch=0 z unchanged");
}

static void test_rotate_yaw_90(void){
    float yaw = (float)M_PI * 0.5f;
    Vec3 p = make_vec3(1.0f, 0.0f, 0.0f);
    Vec3 q = rotate_yaw_pitch(p, yaw, 0.0f);
    assert_near(q.x, 0.0f, 1e-4f, "yaw 90deg x ~ 0");
    assert_near(q.z, -1.0f, 1e-4f, "yaw 90deg z ~ -1");
}

int main(void){
    test_clampf();
    test_rotate_identity();
    test_rotate_yaw_90();

    if(tests_failed == 0){
        printf("[OK] %d tests passed\n", tests_run);
        return 0;
    }

    printf("[FAIL] %d/%d tests failed\n", tests_failed, tests_run);
    return 1;
}
