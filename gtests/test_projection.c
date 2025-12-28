#include <math.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct { float x,y,z; } Vec3;

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
    if(fabsf(a-b) > eps){
        tests_failed++;
        printf("[FAIL] %s (got %.6f expected %.6f)\n", msg, a, b);
    }
}

static void project_to_screen(Vec3 p, float zoom, int cx, int cy, int *sx, int *sy, float *depth){
    float perspective = 900.0f / (900.0f + p.z);
    *sx = (int)lroundf(cx + p.x * zoom * perspective);
    *sy = (int)lroundf(cy - p.y * zoom * perspective);
    *depth = p.z;
}

int main(void){
    int sx1, sy1, sx2, sy2;
    float d1, d2;

    Vec3 nearP = { 10.0f, 0.0f, 0.0f };
    Vec3 farP  = { 10.0f, 0.0f, 500.0f };

    project_to_screen(nearP, 100.0f, 800, 450, &sx1, &sy1, &d1);
    project_to_screen(farP,  100.0f, 800, 450, &sx2, &sy2, &d2);

    assert_true(d1 == 0.0f, "depth near correct");
    assert_true(d2 == 500.0f, "depth far correct");

    // far point should appear closer to center due to smaller perspective factor
    assert_true(abs(sx2 - 800) < abs(sx1 - 800), "perspective shrinks with z");
    assert_near((float)sy1, 450.0f, 1.0f, "y stays centered when y=0");

    if(tests_failed == 0){
        printf("[OK] %d tests passed\n", tests_run);
        return 0;
    }
    printf("[FAIL] %d/%d tests failed\n", tests_failed, tests_run);
    return 1;
}
