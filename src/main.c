/*
 * G3 Town Explorer - prototype v0.1
 * Power Mac G3 (Blue & White) / Mac OS X 10.3.9 / ATI Rage 128 target.
 * Fixed-function OpenGL 1.2 + GLUT only. No textures, no shaders.
 * Walk around a small seaside town and look at the scenery. Nothing else.
 *
 * Build on Panther:
 *   gcc -O2 -o TownExplorer src/main.c -framework GLUT -framework OpenGL -framework Foundation
 * Build on Linux (check only):
 *   gcc -O2 -o TownExplorer src/main.c -lglut -lGL -lGLU -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- presets: same town, different light ---------- */
typedef struct {
    const char *name;
    float skyTop[3];
    float skyHorizon[3];
    float fog[3];
    float fogStart, fogEnd;
    float sea[3];
    float ground[3];
    float light[3];      /* directional light colour */
    float ambient;       /* ambient strength 0..1 */
    float windowGlow;    /* 0 = windows dark, 1 = fully lit */
    float starAlpha;     /* night stars */
    float cloudTint[3];
    float sunPos[2];     /* x,y in sky screen space -1..1 */
    float sunCol[3];
    float sunSize;
} Preset;

static const Preset PRESETS[5] = {
    /* 1: summer day */
    { "1: Summer Day",
      {0.12f,0.44f,0.85f}, {0.56f,0.83f,1.00f}, {0.60f,0.85f,0.96f}, 60.0f, 220.0f,
      {0.09f,0.36f,0.62f}, {0.42f,0.55f,0.32f},
      {1.0f,0.98f,0.90f}, 0.55f, 0.0f, 0.0f,
      {1.0f,1.0f,1.0f}, {-0.55f,0.62f}, {1.0f,0.95f,0.75f}, 0.10f },
    /* 2: blue hour */
    { "2: Blue Hour",
      {0.09f,0.14f,0.28f}, {0.95f,0.62f,0.40f}, {0.17f,0.25f,0.38f}, 45.0f, 190.0f,
      {0.07f,0.16f,0.24f}, {0.25f,0.30f,0.22f},
      {0.75f,0.72f,0.85f}, 0.38f, 0.9f, 0.25f,
      {0.85f,0.75f,0.85f}, {-0.80f,0.10f}, {1.0f,0.62f,0.35f}, 0.16f },
    /* 3: sunset */
    { "3: Sunset",
      {0.17f,0.16f,0.29f}, {0.95f,0.55f,0.22f}, {0.78f,0.43f,0.32f}, 50.0f, 200.0f,
      {0.24f,0.19f,0.29f}, {0.32f,0.30f,0.22f},
      {1.0f,0.75f,0.55f}, 0.42f, 1.0f, 0.0f,
      {1.0f,0.80f,0.70f}, {-0.72f,0.18f}, {1.0f,0.55f,0.25f}, 0.20f },
    /* 4: after rain */
    { "4: After Rain",
      {0.23f,0.28f,0.34f}, {0.56f,0.64f,0.69f}, {0.40f,0.47f,0.52f}, 30.0f, 140.0f,
      {0.16f,0.21f,0.26f}, {0.28f,0.32f,0.27f},
      {0.80f,0.85f,0.88f}, 0.50f, 0.55f, 0.0f,
      {0.62f,0.66f,0.68f}, {0.30f,0.55f}, {0.85f,0.88f,0.88f}, 0.08f },
    /* 5: night */
    { "5: Night",
      {0.03f,0.04f,0.08f}, {0.09f,0.13f,0.22f}, {0.05f,0.07f,0.13f}, 40.0f, 170.0f,
      {0.02f,0.05f,0.09f}, {0.10f,0.13f,0.10f},
      {0.55f,0.62f,0.80f}, 0.25f, 1.0f, 1.0f,
      {0.85f,0.36f,0.18f}, {0.62f,0.66f}, {0.92f,0.95f,1.0f}, 0.06f }
};
static int g_preset = 1; /* start at blue hour */

/* ---------- player ---------- */
static float g_px = 0.0f, g_pz = 18.0f;      /* start on the town side, facing the sea/station */
static float g_yaw = 180.0f;                  /* degrees, 0 = -Z */
static float g_pitch = 0.0f;
static float g_bob = 0.0f;
static int g_keys[256];
static int g_special[256];
static int g_lastMouseX = -1, g_lastMouseY = -1;
static int g_mouseLook = 0;
static double g_timeSec = 0.0;

/* deterministic pseudo-random for stars / grass */
static unsigned long g_rng = 22222UL;
static float frand(void) {
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (float)((g_rng >> 16) & 0x7FFF) / 32767.0f;
}

/* ---------- small drawing helpers ---------- */
static void setCol(const float c[3]) { glColor3f(c[0], c[1], c[2]); }
static void col3(float r, float g, float b) { glColor3f(r, g, b); }
static void colMul(const float c[3], float m) { glColor3f(c[0]*m, c[1]*m, c[2]*m); }

/* axis-aligned box centred at (x,y,z) with size (sx,sy,sz); y is centre height */
static void box(float x, float y, float z, float sx, float sy, float sz) {
    float x0=x-sx*0.5f, x1=x+sx*0.5f, y0=y-sy*0.5f, y1=y+sy*0.5f, z0=z-sz*0.5f, z1=z+sz*0.5f;
    glBegin(GL_QUADS);
    glNormal3f(0,0,1);
    glVertex3f(x0,y0,z1); glVertex3f(x1,y0,z1); glVertex3f(x1,y1,z1); glVertex3f(x0,y1,z1);
    glNormal3f(0,0,-1);
    glVertex3f(x1,y0,z0); glVertex3f(x0,y0,z0); glVertex3f(x0,y1,z0); glVertex3f(x1,y1,z0);
    glNormal3f(1,0,0);
    glVertex3f(x1,y0,z1); glVertex3f(x1,y0,z0); glVertex3f(x1,y1,z0); glVertex3f(x1,y1,z1);
    glNormal3f(-1,0,0);
    glVertex3f(x0,y0,z0); glVertex3f(x0,y0,z1); glVertex3f(x0,y1,z1); glVertex3f(x0,y1,z0);
    glNormal3f(0,1,0);
    glVertex3f(x0,y1,z1); glVertex3f(x1,y1,z1); glVertex3f(x1,y1,z0); glVertex3f(x0,y1,z0);
    glNormal3f(0,-1,0);
    glVertex3f(x0,y0,z0); glVertex3f(x1,y0,z0); glVertex3f(x1,y0,z1); glVertex3f(x0,y0,z1);
    glEnd();
}

/* flat quad on the ground */
static void groundQuad(float x0, float z0, float x1, float z1, float y) {
    glBegin(GL_QUADS);
    glNormal3f(0,1,0);
    glVertex3f(x0,y,z0); glVertex3f(x0,y,z1); glVertex3f(x1,y,z1); glVertex3f(x1,y,z0);
    glEnd();
}

/* vertical quad facing +Z (for window glow, signs, glow sprites) */
static void flatQuadZ(float x, float y, float z, float w, float h) {
    glBegin(GL_QUADS);
    glNormal3f(0,0,1);
    glVertex3f(x-w*0.5f, y-h*0.5f, z);
    glVertex3f(x+w*0.5f, y-h*0.5f, z);
    glVertex3f(x+w*0.5f, y+h*0.5f, z);
    glVertex3f(x-w*0.5f, y+h*0.5f, z);
    glEnd();
}

/* ---------- sky: gradient + sun/moon + stars, drawn in screen space ---------- */
static void drawSky(void) {
    const Preset *p = &PRESETS[g_preset];
    int i;
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG);

    /* gradient: 8 horizontal bands from horizon to top */
    for (i = 0; i < 8; i++) {
        float t0 = (float)i / 8.0f, t1 = (float)(i + 1) / 8.0f;
        float y0 = -0.25f + t0 * 1.25f, y1 = -0.25f + t1 * 1.25f;
        float a = powf(t0, 0.7f), b = powf(t1, 0.7f);
        glBegin(GL_QUADS);
        glColor3f(p->skyHorizon[0] + (p->skyTop[0]-p->skyHorizon[0]) * a,
                  p->skyHorizon[1] + (p->skyTop[1]-p->skyHorizon[1]) * a,
                  p->skyHorizon[2] + (p->skyTop[2]-p->skyHorizon[2]) * a);
        glVertex2f(-1, y0); glVertex2f(1, y0);
        glColor3f(p->skyHorizon[0] + (p->skyTop[0]-p->skyHorizon[0]) * b,
                  p->skyHorizon[1] + (p->skyTop[1]-p->skyHorizon[1]) * b,
                  p->skyHorizon[2] + (p->skyTop[2]-p->skyHorizon[2]) * b);
        glVertex2f(1, y1); glVertex2f(-1, y1);
        glEnd();
    }
    /* below-horizon band (sea haze) */
    glBegin(GL_QUADS);
    setCol(p->skyHorizon);
    glVertex2f(-1,-1); glVertex2f(1,-1); glVertex2f(1,-0.25f); glVertex2f(-1,-0.25f);
    glEnd();

    /* stars */
    if (p->starAlpha > 0.01f) {
        g_rng = 999UL;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for (i = 0; i < 220; i++) {
            float sx = frand() * 2.0f - 1.0f;
            float sy = frand() * 0.95f + 0.02f;
            float tw = 0.4f + 0.6f * frand();
            glColor4f(1, 1, 1, p->starAlpha * tw);
            glVertex2f(sx, sy);
        }
        glEnd();
        /* faint milky-way band: a soft diagonal strip of denser stars */
        if (p->starAlpha > 0.5f) {
            int j;
            glBegin(GL_POINTS);
            for (j = 0; j < 260; j++) {
                float t = frand();
                float bx = -0.9f + t * 1.6f;
                float by = 0.95f - t * 0.75f + (frand() - 0.5f) * 0.16f;
                glColor4f(0.85f, 0.90f, 1.0f, p->starAlpha * (0.12f + 0.25f * frand()));
                glVertex2f(bx, by);
            }
            glEnd();
        }
        glDisable(GL_BLEND);
    }

    /* sun / moon disc with soft halo */
    {
        float sx = p->sunPos[0], sy = p->sunPos[1], s = p->sunSize;
        int k;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (k = 3; k >= 1; k--) {
            float r = s * (float)k, a = 0.10f;
            int j;
            glColor4f(p->sunCol[0], p->sunCol[1], p->sunCol[2], a);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(sx, sy);
            for (j = 0; j <= 16; j++) {
                float ang = (float)j / 16.0f * (float)(2.0 * M_PI);
                glVertex2f(sx + cosf(ang) * r, sy + sinf(ang) * r * 1.4f);
            }
            glEnd();
        }
        glDisable(GL_BLEND);
        glColor3f(p->sunCol[0], p->sunCol[1], p->sunCol[2]);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(sx, sy);
        for (i = 0; i <= 16; i++) {
            float ang = (float)i / 16.0f * (float)(2.0 * M_PI);
            glVertex2f(sx + cosf(ang) * s, sy + sinf(ang) * s * 1.4f);
        }
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

/* ---------- clouds: clusters of low-segment spheres, tinted by preset ---------- */
static void drawCloudCluster(float x, float y, float z, float scale, int seed) {
    const Preset *p = &PRESETS[g_preset];
    int i, n = 6;
    g_rng = (unsigned long)seed;
    glDisable(GL_LIGHTING); /* flat tint reads better at this size */
    for (i = 0; i < n; i++) {
        float ox = (frand() - 0.5f) * 14.0f * scale;
        float oy = (frand() - 0.5f) * 2.0f * scale + (i == 2 ? 3.0f * scale : 0.0f);
        float oz = (frand() - 0.5f) * 5.0f * scale;
        float r = (3.0f + frand() * 3.5f) * scale;
        float shade = 0.82f + frand() * 0.18f;
        /* underside slightly warmed by the preset cloud tint */
        glColor3f(p->cloudTint[0] * shade, p->cloudTint[1] * shade, p->cloudTint[2] * shade);
        glPushMatrix();
        glTranslatef(x + ox, y + oy, z + oz);
        glScalef(1.4f, 0.55f, 1.0f);
        glutSolidSphere(r, 8, 6);
        glPopMatrix();
    }
    glEnable(GL_LIGHTING);
}

static void drawClouds(void) {
    float drift = (float)fmod(g_timeSec * 0.35, 40.0);
    drawCloudCluster(-70.0f + drift, 34.0f, -120.0f, 1.6f, 11);  /* big cumulonimbus over the sea */
    drawCloudCluster(30.0f + drift, 40.0f, -140.0f, 1.1f, 23);
    drawCloudCluster(85.0f + drift, 36.0f, -110.0f, 0.9f, 37);
    drawCloudCluster(-20.0f + drift, 44.0f, -150.0f, 1.3f, 51);
}

/* ---------- sea: one plane + moving sparkle strips + sunset path ---------- */
static void drawSea(void) {
    const Preset *p = &PRESETS[g_preset];
    int i;
    setCol(p->sea);
    groundQuad(-220.0f, -240.0f, 220.0f, -26.0f, -0.05f);

    /* sunset / moonlight path on the water, under the sun */
    if (g_preset == 2 || g_preset == 3 || g_preset == 5) {
        float cx = PRESETS[g_preset].sunPos[0] * 90.0f;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_LIGHTING);
        for (i = 0; i < 7; i++) {
            float z = -32.0f - (float)i * 16.0f;
            float w = 3.0f + (float)i * 2.4f;
            float a = 0.20f - (float)i * 0.02f;
            glColor4f(p->sunCol[0], p->sunCol[1], p->sunCol[2], a);
            groundQuad(cx - w, z - 1.2f, cx + w, z + 1.2f, 0.02f);
        }
        glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    }

    /* drifting sparkle strips */
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (i = 0; i < 14; i++) {
        float speed = 0.6f + (float)(i % 4) * 0.25f;
        float x = -200.0f + (float)fmod((double)i * 31.7 + g_timeSec * 3.0 * speed, 400.0);
        float z = -34.0f - (float)((i * 13) % 90);
        float len = 5.0f + (float)(i % 5) * 2.0f;
        glColor4f(1, 1, 1, 0.10f + 0.06f * (float)(i % 3));
        groundQuad(x, z, x + len, z + 0.5f, 0.03f);
    }
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    /* sailboat, slow drift */
    {
        float bx = -40.0f + (float)fmod(g_timeSec * 0.55, 110.0);
        glPushMatrix();
        glTranslatef(bx, 0.0f, -95.0f);
        col3(0.16f, 0.12f, 0.10f); box(0, 0.5f, 0, 4.5f, 0.9f, 1.4f);      /* hull */
        col3(0.10f, 0.08f, 0.06f); box(0, 2.6f, 0, 0.18f, 4.4f, 0.18f);    /* mast */
        col3(0.92f, 0.90f, 0.85f);
        glBegin(GL_TRIANGLES);                                            /* sail */
        glNormal3f(0,0,1);
        glVertex3f(0.2f, 1.0f, 0); glVertex3f(0.2f, 4.6f, 0); glVertex3f(2.4f, 1.0f, 0);
        glEnd();
        if (g_preset == 5) { /* small warm lamp at the mast top, like the photo */
            glDisable(GL_LIGHTING);
            col3(1.0f, 0.55f, 0.25f); flatQuadZ(0, 4.9f, 0.2f, 0.5f, 0.5f);
            glEnable(GL_LIGHTING);
        }
        glPopMatrix();
    }
}

/* ---------- buildings ---------- */
static void drawHouse(float x, float z, float w, float h, float d,
                      float wr, float wg, float wb, float rr, float rg, float rb,
                      int lit) {
    const Preset *p = &PRESETS[g_preset];
    col3(wr, wg, wb);
    box(x, h * 0.5f, z, w, h, d);
    /* prism roof: two sloped quads + two end triangles */
    col3(rr, rg, rb);
    {
        float x0 = x - w * 0.58f, x1 = x + w * 0.58f;
        float z0 = z - d * 0.5f, z1 = z + d * 0.5f;
        float yb = h, yt = h + w * 0.28f;
        glBegin(GL_QUADS);
        glNormal3f(0, 0.6f, -0.8f);
        glVertex3f(x0, yb, z0); glVertex3f(x1, yb, z0); glVertex3f(x1, yt, z); glVertex3f(x0, yt, z);
        glNormal3f(0, 0.6f, 0.8f);
        glVertex3f(x0, yb, z1); glVertex3f(x0, yt, z); glVertex3f(x1, yt, z); glVertex3f(x1, yb, z1);
        glEnd();
        glBegin(GL_TRIANGLES);
        glNormal3f(-1, 0, 0);
        glVertex3f(x0, yb, z0); glVertex3f(x0, yt, z); glVertex3f(x0, yb, z1);
        glNormal3f(1, 0, 0);
        glVertex3f(x1, yb, z0); glVertex3f(x1, yb, z1); glVertex3f(x1, yt, z);
        glEnd();
    }
    /* TV antenna on the roof: a small silhouette that sells the era */
    glDisable(GL_LIGHTING);
    glColor3f(0.15f, 0.15f, 0.16f);
    glBegin(GL_LINES);
    glVertex3f(x + w * 0.25f, h + w * 0.28f, z);
    glVertex3f(x + w * 0.25f, h + w * 0.28f + 1.6f, z);
    glVertex3f(x + w * 0.25f - 0.8f, h + w * 0.28f + 1.3f, z);
    glVertex3f(x + w * 0.25f + 0.8f, h + w * 0.28f + 1.3f, z);
    glVertex3f(x + w * 0.25f - 0.6f, h + w * 0.28f + 0.9f, z);
    glVertex3f(x + w * 0.25f + 0.6f, h + w * 0.28f + 0.9f, z);
    glEnd();
    glEnable(GL_LIGHTING);
    /* windows on the sea-facing side (-Z) */
    {
        int i, n = (int)(w / 2.2f);
        for (i = 0; i < n; i++) {
            float wx = x - w * 0.5f + 1.2f + (float)i * 2.2f;
            if (lit && p->windowGlow > 0.05f) {
                glDisable(GL_LIGHTING);
                glColor3f(1.0f * p->windowGlow, 0.78f * p->windowGlow, 0.45f * p->windowGlow);
                flatQuadZ(wx, h * 0.55f, z - d * 0.5f - 0.06f, 0.9f, 1.1f);
                glEnable(GL_LIGHTING);
            } else {
                col3(0.20f, 0.26f, 0.32f);
                flatQuadZ(wx, h * 0.55f, z - d * 0.5f - 0.06f, 0.9f, 1.1f);
            }
        }
    }
}

/* the little seaside shop from the photos: awning, sign, warm interior */
static void drawShop(float x, float z) {
    const Preset *p = &PRESETS[g_preset];
    drawHouse(x, z, 9.0f, 4.2f, 6.0f, 0.82f, 0.78f, 0.70f, 0.35f, 0.30f, 0.28f, 1);
    /* awning */
    col3(0.25f, 0.42f, 0.38f);
    box(x, 2.9f, z - 3.6f, 8.4f, 0.18f, 1.6f);
    /* sign board */
    col3(0.92f, 0.90f, 0.84f);
    flatQuadZ(x, 3.8f, z - 3.06f, 4.6f, 0.8f);
    /* doorway glow */
    if (p->windowGlow > 0.05f) {
        glDisable(GL_LIGHTING);
        glColor3f(1.0f * p->windowGlow, 0.80f * p->windowGlow, 0.50f * p->windowGlow);
        flatQuadZ(x - 2.4f, 1.1f, z - 3.06f, 1.4f, 2.2f);
        glEnable(GL_LIGHTING);
    }
    /* outdoor AC unit + stacked crates: lived-in outline */
    col3(0.72f, 0.74f, 0.75f);
    box(x - 5.2f, 0.55f, z - 2.2f, 1.3f, 1.1f, 0.7f);
    col3(0.30f, 0.45f, 0.55f);
    box(x + 3.4f, 0.35f, z - 3.4f, 1.0f, 0.7f, 1.0f);
    box(x + 3.4f, 1.05f, z - 3.4f, 0.9f, 0.65f, 0.9f);
    /* vending machine beside the shop: tiny but it sells the scene */
    col3(0.75f, 0.15f, 0.15f);
    box(x + 5.6f, 0.95f, z - 2.4f, 1.1f, 1.9f, 0.8f);
    if (p->windowGlow > 0.05f) {
        glDisable(GL_LIGHTING);
        glColor3f(0.95f * p->windowGlow, 0.97f * p->windowGlow, 1.0f * p->windowGlow);
        flatQuadZ(x + 5.6f, 1.25f, z - 2.82f, 0.8f, 1.1f);
        glEnable(GL_LIGHTING);
    }
}

/* ---------- poles and wires ---------- */
static void drawPole(float x, float z) {
    col3(0.30f, 0.26f, 0.22f);
    box(x, 3.6f, z, 0.28f, 7.2f, 0.28f);
    box(x, 6.6f, z, 2.4f, 0.22f, 0.22f);
}
static void drawWire(float x0, float x1, float y, float z) {
    int i;
    glDisable(GL_LIGHTING);
    glColor3f(0.08f, 0.08f, 0.10f);
    glBegin(GL_LINE_STRIP);
    for (i = 0; i <= 8; i++) {
        float t = (float)i / 8.0f;
        float sag = sinf(t * (float)M_PI) * 0.55f;
        glVertex3f(x0 + (x1 - x0) * t, y - sag, z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

/* ---------- railway: platform, rails, signal, train ---------- */
static void drawRailway(void) {
    const Preset *p = &PRESETS[g_preset];
    int i;
    /* platform (town side, +Z of the track) */
    colMul(p->ground, 1.25f);
    box(0, 0.45f, 6.5f, 190.0f, 0.9f, 4.6f);
    /* platform edge line */
    glDisable(GL_LIGHTING);
    col3(0.85f, 0.78f, 0.45f);
    groundQuad(-95.0f, 4.55f, 95.0f, 4.95f, 0.92f);
    glEnable(GL_LIGHTING);
    /* wet sheen on the platform after rain: fake reflection of the sky */
    if (g_preset == 4) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_LIGHTING);
        glColor4f(0.75f, 0.82f, 0.88f, 0.28f);
        groundQuad(-95.0f, 4.4f, 95.0f, 8.6f, 0.93f);
        glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    }
    /* ballast + sleepers + rails */
    col3(0.32f, 0.30f, 0.28f);
    groundQuad(-110.0f, -1.8f, 110.0f, 3.8f, 0.02f);
    col3(0.35f, 0.27f, 0.20f);
    for (i = 0; i < 90; i++) {
        float x = -108.0f + (float)i * 2.45f;
        box(x, 0.08f, 1.0f, 0.5f, 0.12f, 3.0f);
    }
    col3(0.62f, 0.62f, 0.65f);
    box(0, 0.18f, 0.25f, 220.0f, 0.10f, 0.14f);
    box(0, 0.18f, 1.75f, 220.0f, 0.10f, 0.14f);

    /* red signal at x=28, with a fake wet reflection streak on the platform */
    col3(0.15f, 0.15f, 0.16f);
    box(28.0f, 1.6f, 3.4f, 0.22f, 3.2f, 0.22f);
    box(28.0f, 3.3f, 3.4f, 0.7f, 1.0f, 0.5f);
    glDisable(GL_LIGHTING);
    col3(1.0f, 0.12f, 0.08f);
    flatQuadZ(28.0f, 3.45f, 3.12f, 0.34f, 0.34f);
    if (g_preset == 4 || g_preset == 5 || g_preset == 2) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 0.15f, 0.10f, g_preset == 4 ? 0.40f : 0.18f);
        groundQuad(27.6f, 4.6f, 28.4f, 8.4f, 0.94f); /* streak toward the viewer */
        glDisable(GL_BLEND);
    }
    glEnable(GL_LIGHTING);

    /* bench on the platform: sitting and watching trains is the game */
    col3(0.42f, 0.32f, 0.22f);
    box(-2.0f, 1.25f, 7.6f, 3.4f, 0.14f, 0.55f);
    box(-2.0f, 1.75f, 7.95f, 3.4f, 0.55f, 0.12f);
    col3(0.20f, 0.20f, 0.22f);
    box(-3.4f, 0.95f, 7.6f, 0.16f, 0.5f, 0.5f);
    box(-0.6f, 0.95f, 7.6f, 0.16f, 0.5f, 0.5f);
    /* station name board hanging from the roof edge */
    col3(0.90f, 0.90f, 0.88f);
    flatQuadZ(-10.0f, 3.05f, 3.85f, 3.6f, 0.7f);
    col3(0.25f, 0.25f, 0.28f);
    flatQuadZ(-10.0f, 3.05f, 3.84f, 3.3f, 0.12f);

    /* station roof over part of the platform + hanging lamps */
    col3(0.40f, 0.36f, 0.32f);
    box(-10.0f, 3.6f, 6.5f, 44.0f, 0.25f, 5.6f);
    for (i = 0; i < 5; i++) {
        float x = -28.0f + (float)i * 9.0f;
        col3(0.30f, 0.28f, 0.26f);
        box(x, 1.8f, 8.4f, 0.25f, 3.6f, 0.25f);
        if (p->windowGlow > 0.05f) {
            glDisable(GL_LIGHTING);
            glColor3f(1.0f * p->windowGlow, 0.85f * p->windowGlow, 0.60f * p->windowGlow);
            flatQuadZ(x, 3.30f, 6.4f, 0.7f, 0.35f);
            glEnable(GL_LIGHTING);
        }
    }
}

static void drawTrain(void) {
    const Preset *p = &PRESETS[g_preset];
    /* 44 second loop: approaches, passes, gone, wait */
    double cycle = fmod(g_timeSec, 44.0);
    float tx;
    int car, w;
    if (cycle > 26.0) return; /* no train: the quiet between trains matters */
    tx = -170.0f + (float)(cycle * 13.0); /* ~47 km/h in scale */
    for (car = 0; car < 3; car++) {
        float cx = tx - (float)car * 13.2f;
        /* Enoden-like green / cream two-tone body */
        col3(0.16f, 0.38f, 0.28f);
        box(cx, 1.95f, 1.0f, 12.6f, 2.5f, 2.6f);
        col3(0.88f, 0.84f, 0.72f);
        box(cx, 2.75f, 1.0f, 12.6f, 0.9f, 2.62f);
        col3(0.25f, 0.25f, 0.27f);
        box(cx, 3.45f, 1.0f, 12.2f, 0.35f, 2.3f);   /* roof */
        box(cx, 0.45f, 1.0f, 12.0f, 0.5f, 2.2f);    /* underframe */
        /* windows: lit warm in the evening */
        for (w = 0; w < 5; w++) {
            float wx = cx - 5.0f + (float)w * 2.5f;
            if (p->windowGlow > 0.05f) {
                glDisable(GL_LIGHTING);
                glColor3f(1.0f * p->windowGlow, 0.82f * p->windowGlow, 0.52f * p->windowGlow);
                flatQuadZ(wx, 2.35f, -0.32f, 1.5f, 0.85f);
                glEnable(GL_LIGHTING);
            } else {
                col3(0.55f, 0.65f, 0.72f);
                flatQuadZ(wx, 2.35f, -0.32f, 1.5f, 0.85f);
            }
        }
    }
    /* pantograph on the front car: two thin crossed bars */
    glDisable(GL_LIGHTING);
    glColor3f(0.10f, 0.10f, 0.11f);
    glBegin(GL_LINES);
    glVertex3f(tx - 1.2f, 3.62f, 1.0f); glVertex3f(tx + 0.4f, 4.45f, 1.0f);
    glVertex3f(tx + 0.4f, 4.45f, 1.0f); glVertex3f(tx + 1.6f, 3.62f, 1.0f);
    glVertex3f(tx - 0.6f, 4.45f, 1.0f); glVertex3f(tx + 1.0f, 4.45f, 1.0f);
    glEnd();
    glEnable(GL_LIGHTING);
    /* headlight on the front car, facing +X travel direction */
    if (p->windowGlow > 0.05f) {
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 0.9f, 0.7f, 0.85f);
        flatQuadZ(tx + 6.35f, 2.2f, -0.32f, 0.5f, 0.5f);
        glColor4f(1.0f, 0.85f, 0.6f, 0.20f);
        flatQuadZ(tx + 6.40f, 2.2f, -0.32f, 1.6f, 1.6f);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }
    (void)p;
}

/* ---------- vegetation & small things ---------- */
static void drawTree(float x, float z, float s) {
    col3(0.36f, 0.28f, 0.20f);
    box(x, 0.9f * s, z, 0.3f * s, 1.8f * s, 0.3f * s);
    col3(0.20f, 0.38f, 0.20f);
    glPushMatrix();
    glTranslatef(x, 2.6f * s, z);
    glutSolidCone(1.5f * s, 2.8f * s, 8, 2);
    glPopMatrix();
}
static void drawGrassTufts(void) {
    int i;
    glDisable(GL_LIGHTING);
    col3(0.30f, 0.48f, 0.22f);
    g_rng = 4242UL;
    glBegin(GL_TRIANGLES);
    for (i = 0; i < 260; i++) {
        /* mostly along the rural east side and the track */
        float x = 30.0f + frand() * 75.0f;
        float z = -4.0f + frand() * 14.0f;
        float h = 0.5f + frand() * 0.6f;
        if (i % 3 == 0) { x = -100.0f + frand() * 200.0f; z = 10.0f + frand() * 8.0f; }
        glVertex3f(x - 0.18f, 0.0f, z);
        glVertex3f(x + 0.18f, 0.0f, z);
        glVertex3f(x, h, z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}
static void drawBicycle(float x, float z) {
    int i;
    glDisable(GL_LIGHTING);
    glColor3f(0.12f, 0.12f, 0.14f);
    /* two wheel circles + frame lines, leaning by the fence near the rural road */
    for (i = 0; i < 2; i++) {
        float cx = x + (i == 0 ? -0.55f : 0.55f);
        int j;
        glBegin(GL_LINE_LOOP);
        for (j = 0; j < 14; j++) {
            float a = (float)j / 14.0f * (float)(2.0 * M_PI);
            glVertex3f(cx + cosf(a) * 0.34f, 0.34f + sinf(a) * 0.34f, z);
        }
        glEnd();
    }
    glBegin(GL_LINES);
    glVertex3f(x - 0.55f, 0.34f, z); glVertex3f(x, 0.78f, z);
    glVertex3f(x, 0.78f, z); glVertex3f(x + 0.55f, 0.34f, z);
    glVertex3f(x - 0.55f, 0.34f, z); glVertex3f(x + 0.10f, 0.82f, z);
    glEnd();
    glEnable(GL_LIGHTING);
}

/* ---------- the whole town ---------- */
static void drawTown(void) {
    const Preset *p = &PRESETS[g_preset];
    int i;
    /* base ground */
    setCol(p->ground);
    groundQuad(-220.0f, -26.0f, 220.0f, 90.0f, 0.0f);
    /* distant hill behind the town */
    colMul(p->ground, 0.75f);
    glBegin(GL_TRIANGLES);
    glNormal3f(0, 0.3f, -1);
    glVertex3f(-230.0f, 0.0f, 88.0f); glVertex3f(230.0f, 0.0f, 88.0f); glVertex3f(0.0f, 42.0f, 95.0f);
    glEnd();

    drawSea();

    /* seaside road between the track and the sea */
    col3(0.23f, 0.23f, 0.25f);
    groundQuad(-160.0f, -14.0f, 160.0f, -4.5f, 0.04f);
    glDisable(GL_LIGHTING);
    col3(0.80f, 0.72f, 0.40f);
    for (i = 0; i < 22; i++) groundQuad(-150.0f + (float)i * 14.0f, -9.45f, -144.0f + (float)i * 14.0f, -9.05f, 0.05f);
    glEnable(GL_LIGHTING);
    /* sea fence / railing */
    col3(0.55f, 0.57f, 0.60f);
    for (i = 0; i < 40; i++) box(-156.0f + (float)i * 8.0f, 0.55f, -15.2f, 0.16f, 1.1f, 0.16f);
    box(0, 1.05f, -15.2f, 320.0f, 0.12f, 0.12f);
    box(0, 0.55f, -15.2f, 320.0f, 0.08f, 0.08f);
    /* sand strip */
    col3(0.76f, 0.70f, 0.55f);
    groundQuad(-220.0f, -26.0f, 220.0f, -16.0f, 0.01f);

    /* concrete breakwater blocks scattered along the waterline */
    col3(0.52f, 0.53f, 0.54f);
    for (i = 0; i < 16; i++) {
        float bx = -140.0f + (float)i * 18.0f + (float)(i % 3) * 2.0f;
        box(bx, 0.35f, -22.5f - (float)(i % 2) * 1.6f, 2.2f, 1.1f, 2.2f);
    }

    drawRailway();
    drawTrain();

    /* town-side street */
    col3(0.26f, 0.26f, 0.28f);
    groundQuad(-160.0f, 10.5f, 160.0f, 16.5f, 0.03f);

    /* houses along the back street */
    drawHouse(-72.0f, 24.0f, 8, 5.0f, 6, 0.78f,0.72f,0.62f, 0.45f,0.30f,0.25f, 1);
    drawHouse(-58.0f, 25.0f, 7, 4.4f, 6, 0.65f,0.70f,0.75f, 0.30f,0.32f,0.38f, 0);
    drawHouse(-44.0f, 24.0f, 9, 5.6f, 6, 0.85f,0.80f,0.70f, 0.50f,0.35f,0.22f, 1);
    drawShop(-28.0f, 24.5f);                      /* the seaside shop */
    drawHouse(-10.0f, 25.0f, 7, 4.6f, 6, 0.72f,0.66f,0.58f, 0.38f,0.30f,0.26f, 1);
    drawHouse(4.0f, 24.0f, 8, 5.2f, 6, 0.68f,0.74f,0.66f, 0.33f,0.38f,0.30f, 0);
    drawHouse(18.0f, 25.0f, 7, 4.2f, 6, 0.80f,0.74f,0.66f, 0.46f,0.28f,0.24f, 1);
    drawHouse(34.0f, 24.0f, 9, 5.4f, 6, 0.74f,0.70f,0.78f, 0.36f,0.34f,0.42f, 1);
    drawHouse(52.0f, 25.0f, 8, 4.8f, 6, 0.82f,0.76f,0.64f, 0.42f,0.30f,0.24f, 0);
    drawHouse(70.0f, 24.0f, 7, 4.4f, 6, 0.70f,0.68f,0.60f, 0.34f,0.30f,0.28f, 1);

    /* poles + wires along the street */
    for (i = 0; i < 9; i++) {
        float x = -80.0f + (float)i * 20.0f;
        drawPole(x, 17.5f);
        if (i > 0) drawWire(x - 20.0f, x, 6.6f, 17.5f);
    }

    /* trees, grass, bicycle on the rural east side */
    drawTree(-64.0f, 15.0f, 1.1f); drawTree(-18.0f, 33.0f, 1.3f);
    drawTree(12.0f, 34.0f, 1.0f);  drawTree(44.0f, 33.0f, 1.25f);
    drawTree(82.0f, 18.0f, 1.1f);  drawTree(96.0f, 30.0f, 1.4f);
    drawGrassTufts();
    drawBicycle(58.0f, -3.4f);

    drawClouds();
}

/* ---------- HUD ---------- */
static void drawHud(void) {
    const char *s = PRESETS[g_preset].name;
    const char *hint = "WASD walk / mouse look / 1-5 time / ESC quit";
    int i;
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, 640, 0, 480, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glColor3f(1, 1, 1);
    glRasterPos2i(14, 456);
    for (i = 0; s[i]; i++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, s[i]);
    glColor3f(0.85f, 0.85f, 0.85f);
    glRasterPos2i(14, 14);
    for (i = 0; hint[i]; i++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, hint[i]);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

/* ---------- frame ---------- */
static void display(void) {
    const Preset *p = &PRESETS[g_preset];
    float lightPos[4] = { -0.45f, 0.75f, -0.55f, 0.0f };
    float lightCol[4];
    float amb[4];
    float eyeY = 1.7f + sinf(g_bob) * 0.045f;
    float yawR = g_yaw * (float)M_PI / 180.0f;
    float pitchR = g_pitch * (float)M_PI / 180.0f;
    float lookX = sinf(yawR) * cosf(pitchR);
    float lookY = sinf(pitchR);
    float lookZ = -cosf(yawR) * cosf(pitchR);

    glClearColor(p->fog[0], p->fog[1], p->fog[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSky();

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(62.0, 640.0 / 480.0, 0.1, 420.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(g_px, eyeY, g_pz, g_px + lookX, eyeY + lookY, g_pz + lookZ, 0, 1, 0);

    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    lightCol[0] = p->light[0]; lightCol[1] = p->light[1]; lightCol[2] = p->light[2]; lightCol[3] = 1.0f;
    amb[0] = amb[1] = amb[2] = p->ambient * 0.55f; amb[3] = 1.0f;
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightCol);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, p->fog);
    glFogf(GL_FOG_START, p->fogStart);
    glFogf(GL_FOG_END, p->fogEnd);

    drawTown();
    drawHud();
    glutSwapBuffers();
}

/* ---------- movement ---------- */
static void update(int value) {
    float speed = 6.0f;
    float yawR = g_yaw * (float)M_PI / 180.0f;
    float fx = sinf(yawR), fz = -cosf(yawR);
    float rx = cosf(yawR), rz = sinf(yawR);
    float mx = 0.0f, mz = 0.0f;
    (void)value;
    g_timeSec += 0.033;
    if (g_keys[27]) exit(0);
    if (g_keys['1']) g_preset = 0;
    if (g_keys['2']) g_preset = 1;
    if (g_keys['3']) g_preset = 2;
    if (g_keys['4']) g_preset = 3;
    if (g_keys['5']) g_preset = 4;
    if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) speed = 11.0f;
    if (g_keys['w'] || g_keys['W'] || g_special[GLUT_KEY_UP])    { mx += fx; mz += fz; }
    if (g_keys['s'] || g_keys['S'] || g_special[GLUT_KEY_DOWN])  { mx -= fx; mz -= fz; }
    if (g_keys['a'] || g_keys['A'] || g_special[GLUT_KEY_LEFT])  { mx -= rx; mz -= rz; }
    if (g_keys['d'] || g_keys['D'] || g_special[GLUT_KEY_RIGHT]) { mx += rx; mz += rz; }
    if (mx != 0.0f || mz != 0.0f) {
        float len = sqrtf(mx * mx + mz * mz);
        g_px += (mx / len) * speed * 0.033f;
        g_pz += (mz / len) * speed * 0.033f;
        g_bob += 0.28f;
        /* keep the player on land, out of the sea and off the hill */
        if (g_px < -105.0f) g_px = -105.0f;
        if (g_px > 105.0f) g_px = 105.0f;
        if (g_pz < -13.5f) g_pz = -13.5f;
        if (g_pz > 38.0f) g_pz = 38.0f;
    }
    glutPostRedisplay();
    glutTimerFunc(33, update, 0);
}

static void keyDown(unsigned char k, int x, int y) { (void)x; (void)y; g_keys[k] = 1; }
static void keyUp(unsigned char k, int x, int y) { (void)x; (void)y; g_keys[k] = 0; }
static void specDown(int k, int x, int y) { (void)x; (void)y; if (k >= 0 && k < 256) g_special[k] = 1; }
static void specUp(int k, int x, int y) { (void)x; (void)y; if (k >= 0 && k < 256) g_special[k] = 0; }

static void mouse(int button, int state, int x, int y) {
    (void)button; (void)x; (void)y;
    if (state == GLUT_DOWN) { g_mouseLook = 1; g_lastMouseX = -1; }
}
static void motion(int x, int y) {
    if (!g_mouseLook) return;
    if (g_lastMouseX >= 0) {
        g_yaw += (float)(x - g_lastMouseX) * 0.22f;
        g_pitch -= (float)(y - g_lastMouseY) * 0.18f;
        if (g_pitch > 55.0f) g_pitch = 55.0f;
        if (g_pitch < -40.0f) g_pitch = -40.0f;
    }
    g_lastMouseX = x; g_lastMouseY = y;
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(640, 480);
    glutCreateWindow("G3 Town Explorer");
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specDown);
    glutSpecialUpFunc(specUp);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(motion);
    glutTimerFunc(33, update, 0);
    printf("G3 Town Explorer v0.1 - presets 1..5, WASD to walk\n");
    glutMainLoop();
    return 0;
}
