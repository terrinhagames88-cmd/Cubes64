// Cubos 64 - um clone simples de Minecraft Classic para Nintendo 64.
// Feito com libdragon (branch "preview", que inclui a porta de OpenGL 1.1).
//
// Controles:
//   Analogico ........ andar (frente/tras/lados)
//   Botoes C ......... olhar (esquerda/direita/cima/baixo)
//   A ................ pular
//   Z ................ quebrar bloco
//   R ................ colocar bloco
//   L / B ............ proximo / anterior bloco do inventario

#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------- texturas
enum {
    T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_COBBLE, T_SAND,
    T_LOG_SIDE, T_LOG_TOP, T_PLANKS, T_LEAVES, T_BRICK,
    T_GLASS, T_SNOW, T_WOOL_RED, T_WOOL_BLUE,
    T_COUNT
};

static const char *tex_files[T_COUNT] = {
    "rom:/t_grass_top.sprite", "rom:/t_grass_side.sprite", "rom:/t_dirt.sprite",
    "rom:/t_stone.sprite", "rom:/t_cobble.sprite", "rom:/t_sand.sprite",
    "rom:/t_log_side.sprite", "rom:/t_log_top.sprite", "rom:/t_planks.sprite",
    "rom:/t_leaves.sprite", "rom:/t_brick.sprite", "rom:/t_glass.sprite",
    "rom:/t_snow.sprite", "rom:/t_wool_red.sprite", "rom:/t_wool_blue.sprite",
};

static sprite_t *sprites[T_COUNT];
static GLuint textures[T_COUNT];

// ---------------------------------------------------------------- blocos
enum {
    B_AIR, B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_SAND, B_LOG, B_PLANKS,
    B_LEAVES, B_BRICK, B_GLASS, B_SNOW, B_WOOL_RED, B_WOOL_BLUE,
    B_COUNT
};

// textura de cada bloco: {topo, lado, baixo}
static const uint8_t block_tex[B_COUNT][3] = {
    [B_AIR]       = {0, 0, 0},
    [B_GRASS]     = {T_GRASS_TOP, T_GRASS_SIDE, T_DIRT},
    [B_DIRT]      = {T_DIRT, T_DIRT, T_DIRT},
    [B_STONE]     = {T_STONE, T_STONE, T_STONE},
    [B_COBBLE]    = {T_COBBLE, T_COBBLE, T_COBBLE},
    [B_SAND]      = {T_SAND, T_SAND, T_SAND},
    [B_LOG]       = {T_LOG_TOP, T_LOG_SIDE, T_LOG_TOP},
    [B_PLANKS]    = {T_PLANKS, T_PLANKS, T_PLANKS},
    [B_LEAVES]    = {T_LEAVES, T_LEAVES, T_LEAVES},
    [B_BRICK]     = {T_BRICK, T_BRICK, T_BRICK},
    [B_GLASS]     = {T_GLASS, T_GLASS, T_GLASS},
    [B_SNOW]      = {T_SNOW, T_SNOW, T_SNOW},
    [B_WOOL_RED]  = {T_WOOL_RED, T_WOOL_RED, T_WOOL_RED},
    [B_WOOL_BLUE] = {T_WOOL_BLUE, T_WOOL_BLUE, T_WOOL_BLUE},
};

// inventario (ordem em que L/B percorrem)
static const uint8_t hotbar[] = {
    B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_SAND, B_LOG, B_PLANKS,
    B_LEAVES, B_BRICK, B_GLASS, B_SNOW, B_WOOL_RED, B_WOOL_BLUE,
};
#define HOTBAR_COUNT ((int)(sizeof(hotbar) / sizeof(hotbar[0])))

// ---------------------------------------------------------------- mundo
#define WX 48
#define WY 24
#define WZ 48

static uint8_t world[WY][WZ][WX];

static inline bool in_bounds(int x, int y, int z)
{
    return x >= 0 && x < WX && y >= 0 && y < WY && z >= 0 && z < WZ;
}

static inline uint8_t get_block(int x, int y, int z)
{
    return in_bounds(x, y, z) ? world[y][z][x] : B_AIR;
}

// paredes invisiveis nas bordas e chao infinito para baixo
static inline bool solid_for_collision(int x, int y, int z)
{
    if (y < 0) return true;
    if (x < 0 || x >= WX || z < 0 || z >= WZ) return true;
    if (y >= WY) return false;
    return world[y][z][x] != B_AIR;
}

static int height_at(int x, int z)
{
    float h = 10.0f + 3.0f * sinf(x * 0.19f) + 3.0f * cosf(z * 0.15f)
            + 2.0f * sinf((x + z) * 0.11f);
    int hi = (int)h;
    if (hi < 2) hi = 2;
    if (hi > WY - 8) hi = WY - 8;
    return hi;
}

static uint32_t rng_state = 12345;
static uint32_t rng(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 8;
}

static void gen_world(void)
{
    memset(world, 0, sizeof(world));

    for (int z = 0; z < WZ; z++) {
        for (int x = 0; x < WX; x++) {
            int h = height_at(x, z);
            uint8_t top = B_GRASS;
            if (h <= 9) top = B_SAND;
            else if (h >= 15) top = B_SNOW;

            for (int y = 0; y <= h; y++) {
                uint8_t b;
                if (y == h) b = top;
                else if (y >= h - 3) b = (top == B_SAND) ? B_SAND : B_DIRT;
                else b = B_STONE;
                world[y][z][x] = b;
            }
        }
    }

    // algumas arvores
    for (int i = 0; i < 16; i++) {
        int x = 4 + rng() % (WX - 8);
        int z = 4 + rng() % (WZ - 8);
        int h = height_at(x, z);
        if (world[h][z][x] != B_GRASS) continue;

        for (int y = h + 1; y <= h + 4; y++) world[y][z][x] = B_LOG;
        for (int dy = 3; dy <= 5; dy++) {
            int r = (dy == 5) ? 1 : 2;
            for (int dz = -r; dz <= r; dz++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (r == 2 && dx * dx == 4 && dz * dz == 4) continue;
                    int yy = h + dy;
                    if (!in_bounds(x + dx, yy, z + dz)) continue;
                    if (world[yy][z + dz][x + dx] == B_AIR)
                        world[yy][z + dz][x + dx] = B_LEAVES;
                }
            }
        }
    }
}

// ---------------------------------------------------------------- jogador
#define EYE_HEIGHT 1.6f
#define BODY_HEIGHT 1.75f
#define BODY_RADIUS 0.3f
#define REACH 5.0f

static float px, py, pz;      // posicao dos pes
static float vy;              // velocidade vertical
static bool on_ground;
static float yaw, pitch;      // em radianos
static int selected;          // indice no hotbar

static bool box_hits(float x, float y, float z)
{
    int x0 = (int)floorf(x - BODY_RADIUS), x1 = (int)floorf(x + BODY_RADIUS);
    int z0 = (int)floorf(z - BODY_RADIUS), z1 = (int)floorf(z + BODY_RADIUS);
    int y0 = (int)floorf(y), y1 = (int)floorf(y + BODY_HEIGHT);
    for (int yy = y0; yy <= y1; yy++)
        for (int zz = z0; zz <= z1; zz++)
            for (int xx = x0; xx <= x1; xx++)
                if (solid_for_collision(xx, yy, zz)) return true;
    return false;
}

static void respawn(void)
{
    px = WX / 2 + 0.5f;
    pz = WZ / 2 + 0.5f;
    py = height_at(WX / 2, WZ / 2) + 2.0f;
    vy = 0;
    on_ground = false;
}

static void update_player(float dt, joypad_inputs_t in, joypad_buttons_t held)
{
    // olhar com os botoes C
    const float turn = 2.2f * dt;
    if (held.c_left)  yaw += turn;
    if (held.c_right) yaw -= turn;
    if (held.c_up)    pitch += turn;
    if (held.c_down)  pitch -= turn;
    if (pitch > 1.4f) pitch = 1.4f;
    if (pitch < -1.4f) pitch = -1.4f;

    // andar com o analogico
    float sx = in.stick_x / 80.0f;
    float sy = in.stick_y / 80.0f;
    if (fabsf(sx) < 0.1f) sx = 0;
    if (fabsf(sy) < 0.1f) sy = 0;
    if (sx > 1) sx = 1;
    if (sx < -1) sx = -1;
    if (sy > 1) sy = 1;
    if (sy < -1) sy = -1;

    float fwd_x = -sinf(yaw), fwd_z = -cosf(yaw);
    float right_x = cosf(yaw), right_z = -sinf(yaw);
    const float speed = 4.5f;
    float vx = (fwd_x * sy + right_x * sx) * speed;
    float vz = (fwd_z * sy + right_z * sx) * speed;

    float nx = px + vx * dt;
    if (!box_hits(nx, py, pz)) px = nx;
    float nz = pz + vz * dt;
    if (!box_hits(px, py, nz)) pz = nz;

    // pulo e gravidade
    if (held.a && on_ground) {
        vy = 7.6f;
        on_ground = false;
    }
    vy -= 22.0f * dt;
    if (vy < -30.0f) vy = -30.0f;
    float ny = py + vy * dt;
    if (!box_hits(px, ny, pz)) {
        py = ny;
        on_ground = false;
    } else {
        if (vy < 0) on_ground = true;
        vy = 0;
    }

    if (py < -10.0f) respawn();
}

// ---------------------------------------------------------------- raycast
static bool raycast(int *hx, int *hy, int *hz, int *bx, int *by, int *bz)
{
    float ox = px, oy = py + EYE_HEIGHT, oz = pz;
    float dx = -sinf(yaw) * cosf(pitch);
    float dy = sinf(pitch);
    float dz = -cosf(yaw) * cosf(pitch);

    int prev[3] = { (int)floorf(ox), (int)floorf(oy), (int)floorf(oz) };
    for (float t = 0.0f; t < REACH; t += 0.04f) {
        int cx = (int)floorf(ox + dx * t);
        int cy = (int)floorf(oy + dy * t);
        int cz = (int)floorf(oz + dz * t);
        if (cx == prev[0] && cy == prev[1] && cz == prev[2]) continue;
        if (get_block(cx, cy, cz) != B_AIR) {
            *hx = cx; *hy = cy; *hz = cz;
            *bx = prev[0]; *by = prev[1]; *bz = prev[2];
            return true;
        }
        prev[0] = cx; prev[1] = cy; prev[2] = cz;
    }
    return false;
}

static void break_block(void)
{
    int hx, hy, hz, bx, by, bz;
    if (!raycast(&hx, &hy, &hz, &bx, &by, &bz)) return;
    if (hy == 0) return; // mantem o fundo do mundo
    world[hy][hz][hx] = B_AIR;
}

static void place_block(void)
{
    int hx, hy, hz, bx, by, bz;
    if (!raycast(&hx, &hy, &hz, &bx, &by, &bz)) return;
    if (!in_bounds(bx, by, bz) || world[by][bz][bx] != B_AIR) return;
    world[by][bz][bx] = hotbar[selected];
    if (box_hits(px, py, pz)) world[by][bz][bx] = B_AIR; // nao prende o jogador
}

// ---------------------------------------------------------------- faces
typedef struct {
    int16_t x, y, z;
    uint8_t dir, tex;
} face_t;

#define MAX_FACES 4096
#define DRAW_RADIUS 8

static face_t faces[MAX_FACES];
static int nfaces;
static int tex_count[T_COUNT];

// direcoes: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
static const int8_t dir_n[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

static const int8_t face_v[6][4][3] = {
    { {1, 1, 0}, {1, 1, 1}, {1, 0, 1}, {1, 0, 0} }, // +X
    { {0, 1, 1}, {0, 1, 0}, {0, 0, 0}, {0, 0, 1} }, // -X
    { {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1} }, // +Y
    { {0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0} }, // -Y
    { {0, 1, 1}, {1, 1, 1}, {1, 0, 1}, {0, 0, 1} }, // +Z
    { {1, 1, 0}, {0, 1, 0}, {0, 0, 0}, {1, 0, 0} }, // -Z
};

static const float face_uv[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
static const float face_shade[6] = { 0.80f, 0.80f, 1.00f, 0.55f, 0.90f, 0.90f };

static void collect_faces(float ex, float ey, float ez)
{
    nfaces = 0;
    memset(tex_count, 0, sizeof(tex_count));

    float fx = -sinf(yaw), fz = -cosf(yaw);
    bool back_cull = fabsf(pitch) < 0.7f;

    int cx = (int)floorf(ex), cz = (int)floorf(ez);
    int x0 = cx - DRAW_RADIUS, x1 = cx + DRAW_RADIUS;
    int z0 = cz - DRAW_RADIUS, z1 = cz + DRAW_RADIUS;
    if (x0 < 0) x0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 > WX - 1) x1 = WX - 1;
    if (z1 > WZ - 1) z1 = WZ - 1;

    for (int y = 0; y < WY; y++) {
        for (int z = z0; z <= z1; z++) {
            for (int x = x0; x <= x1; x++) {
                uint8_t b = world[y][z][x];
                if (b == B_AIR) continue;

                float dx = x + 0.5f - ex, dz = z + 0.5f - ez;
                if (dx * dx + dz * dz > (DRAW_RADIUS + 0.5f) * (DRAW_RADIUS + 0.5f)) continue;
                if (back_cull && dx * fx + dz * fz < -2.5f) continue;

                for (int d = 0; d < 6; d++) {
                    uint8_t nb = get_block(x + dir_n[d][0], y + dir_n[d][1], z + dir_n[d][2]);
                    if (!(nb == B_AIR || (nb == B_GLASS && b != B_GLASS))) continue;
                    if (d == 3 && y == 0) continue;

                    // so desenha faces voltadas para a camera
                    switch (d) {
                        case 0: if (ex <= x + 1) continue; break;
                        case 1: if (ex >= x) continue; break;
                        case 2: if (ey <= y + 1) continue; break;
                        case 3: if (ey >= y) continue; break;
                        case 4: if (ez <= z + 1) continue; break;
                        case 5: if (ez >= z) continue; break;
                    }

                    if (nfaces >= MAX_FACES) return;
                    uint8_t t = (d == 2) ? block_tex[b][0]
                              : (d == 3) ? block_tex[b][2]
                                         : block_tex[b][1];
                    faces[nfaces++] = (face_t){ x, y, z, (uint8_t)d, t };
                    tex_count[t]++;
                }
            }
        }
    }
}

static void draw_faces(void)
{
    for (int t = 0; t < T_COUNT; t++) {
        if (!tex_count[t]) continue;
        glBindTexture(GL_TEXTURE_2D, textures[t]);
        glBegin(GL_QUADS);
        for (int i = 0; i < nfaces; i++) {
            const face_t *f = &faces[i];
            if (f->tex != t) continue;
            float s = face_shade[f->dir];
            glColor3f(s, s, s);
            for (int k = 0; k < 4; k++) {
                glTexCoord2f(face_uv[k][0], face_uv[k][1]);
                glVertex3f(f->x + face_v[f->dir][k][0],
                           f->y + face_v[f->dir][k][1],
                           f->z + face_v[f->dir][k][2]);
            }
        }
        glEnd();
    }
}

// ---------------------------------------------------------------- HUD
static void draw_rect(float x0, float y0, float x1, float y1)
{
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, 0);
    glVertex3f(x1, y0, 0);
    glVertex3f(x1, y1, 0);
    glVertex3f(x0, y1, 0);
    glEnd();
}

static void draw_hud(void)
{
    float w = (float)display_get_width();
    float h = (float)display_get_height();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);

    // mira
    glColor3f(1, 1, 1);
    draw_rect(w / 2 - 6, h / 2 - 1, w / 2 + 6, h / 2 + 1);
    draw_rect(w / 2 - 1, h / 2 - 6, w / 2 + 1, h / 2 + 6);

    // moldura + bloco selecionado
    glColor3f(0.1f, 0.1f, 0.15f);
    draw_rect(8, 8, 44, 44);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textures[block_tex[hotbar[selected]][1]]);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(10, 10, 0);
    glTexCoord2f(1, 0); glVertex3f(42, 10, 0);
    glTexCoord2f(1, 1); glVertex3f(42, 42, 0);
    glTexCoord2f(0, 1); glVertex3f(10, 42, 0);
    glEnd();
}

// ---------------------------------------------------------------- render
static const GLfloat sky[4] = { 0.45f, 0.72f, 1.0f, 1.0f };

static void render(void)
{
    surface_t *disp = display_get();
    rdpq_attach(disp, display_get_zbuf());
    gl_context_begin();

    glClearColor(sky[0], sky[1], sky[2], sky[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)display_get_width() / (float)display_get_height();
    const float near_plane = 0.1f, far_plane = 40.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-near_plane * aspect * 0.7f, near_plane * aspect * 0.7f,
              -near_plane * 0.7f, near_plane * 0.7f, near_plane, far_plane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float ex = px, ey = py + EYE_HEIGHT, ez = pz;
    glRotatef(-pitch * (180.0f / (float)M_PI), 1, 0, 0);
    glRotatef(-yaw * (180.0f / (float)M_PI), 0, 1, 0);
    glTranslatef(-ex, -ey, -ez);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_FOG);

    collect_faces(ex, ey, ez);
    draw_faces();

    draw_hud();

    gl_context_end();
    rdpq_detach_show();
}

// ---------------------------------------------------------------- main
static void setup_gl(void)
{
    glGenTextures(T_COUNT, textures);
    for (int i = 0; i < T_COUNT; i++) {
        sprites[i] = sprite_load(tex_files[i]);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSpriteTextureN64(GL_TEXTURE_2D, sprites[i], NULL);
    }

    // vidro: pixels transparentes sao descartados
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);

    // neblina azul-clara esconde o limite da distancia de desenho
    glFogf(GL_FOG_START, 6.0f);
    glFogf(GL_FOG_END, 16.0f);
    glFogfv(GL_FOG_COLOR, sky);
}

int main(void)
{
    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE,
                 FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    rdpq_init();
    gl_init();
    joypad_init();

    setup_gl();
    gen_world();
    respawn();

    uint64_t last = get_ticks_ms();

    while (1) {
        joypad_poll();
        joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
        joypad_buttons_t held = joypad_get_buttons_held(JOYPAD_PORT_1);
        joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        uint64_t now = get_ticks_ms();
        float dt = (float)(now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        if (pressed.l) selected = (selected + 1) % HOTBAR_COUNT;
        if (pressed.b) selected = (selected + HOTBAR_COUNT - 1) % HOTBAR_COUNT;
        if (pressed.z) break_block();
        if (pressed.r) place_block();

        update_player(dt, in, held);
        render();
    }
}
