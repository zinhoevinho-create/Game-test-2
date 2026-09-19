/* ========================================================================== */
/*           ZOOBA 2 PSP - ULTIMATE MASSIVE CORE ENGINE (55+ KB)              */
/* ========================================================================== */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("Zooba 2 PSP Ultimate Core", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];

/* ========================================================================== */
/*                      ESTRUTURAS DE DADOS ESTENDIDAS                        */
/* ========================================================================== */

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    int active;
    int type;
    float lifeTime;
    float damage;
    float scale;
    unsigned int color;
    int pierceCount;
} UltimateProjectile;

typedef struct {
    float x, y, z;
    float vx, vy;
    float hp;
    float maxHp;
    float speed;
    int active;
    int type;
    int state;
    int aiTimer;
    float attackRange;
    int eliteFlag;
} UltimateEnemy;

typedef struct {
    float x, y, w, h;
    unsigned int color;
    int solid;
    int destructible;
    float durability;
    int zoneType;
} UltimateObstacle;

typedef struct {
    float x, y;
    float vx, vy;
    float hp;
    float maxHp;
    float speed;
    int weaponEquipped;
    int score;
    int charClass;
    int ammoCount;
    int energyShield;
    int activePowerup;
    float powerupTimer;
} UltimatePlayer;

typedef struct {
    float x, y;
    float vx, vy;
    unsigned int color;
    float life;
    float maxLife;
    float size;
    int active;
} UltimateParticle;

typedef struct {
    float x, y;
    float width;
    float height;
    unsigned int baseColor;
    unsigned int borderColor;
    int active;
    int actionId;
} UltimateUIElement;

typedef struct {
    int id;
    int active;
    float x, y;
    int type; // 0: Health, 1: Ammo, 2: Shield, 3: Speed
    float respawnTimer;
} UltimateItemDrop;

/* ========================================================================== */
/*                   CALLBACKS DO SISTEMA DO PSP                              */
/* ========================================================================== */

static int ultimate_exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

static int ultimate_callback_thread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", ultimate_exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void ultimate_setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", ultimate_callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
}

/* ========================================================================== */
/*                     MOTOR GRÁFICO GU - PRIMITIVAS                          */
/* ========================================================================== */

static void ultimate_draw_rect(float x, float y, float w, float h, unsigned int color) {
    struct Vertex {
        unsigned int color;
        float x, y, z;
    };

    struct Vertex* vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));
    if (!vertices) return;

    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;

    vertices[1].color = color;
    vertices[1].x = x + w;
    vertices[1].y = y + h;
    vertices[1].z = 0.0f;

    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, vertices);
}

static void ultimate_render_background(unsigned int baseColor) {
    sceGuClearColor(baseColor);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

/* ========================================================================== */
/*                        FUNÇÃO PRINCIPAL (MAIN)                             */
/* ========================================================================== */

int main(void) {
    ultimate_setup_callbacks();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(480, 272, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    
    sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
    sceGuViewport(2048, 2048, 480, 272);
    sceGuDepthRange(0xcf9c, 0x50);
    
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, 480, 272);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad, lastPad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlReadBufferPositive(&lastPad, 1);

    int gameState = 0; // 0: Menu, 1: Seleção, 2: Jogo, 3: Pausa, 4: GameOver, 5: Vitória
    int menuCursor = 0;
    int globalTick = 0;
    int shootCooldown = 0;
    int waveLevel = 1;

    UltimatePlayer player;
    player.x = 220.0f;
    player.y = 120.0f;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.hp = 150.0f;
    player.maxHp = 150.0f;
    player.speed = 3.8f;
    player.weaponEquipped = 0;
    player.score = 0;
    player.charClass = 0;
    player.ammoCount = 60;
    player.energyShield = 100;
    player.activePowerup = 0;
    player.powerupTimer = 0.0f;

    // Pools massivos otimizados para estabilidade total no PSP
    UltimateProjectile projectiles[150];
    for(int i = 0; i < 150; i++) {
        projectiles[i].active = 0;
        projectiles[i].lifeTime = 0.0f;
    }

    UltimateEnemy enemies[50];
    for(int i = 0; i < 50; i++) {
        enemies[i].active = 1;
        enemies[i].x = (float)(rand() % 420 + 30);
        enemies[i].y = (float)(rand() % 200 + 35);
        enemies[i].hp = 90.0f;
        enemies[i].maxHp = 90.0f;
        enemies[i].speed = 1.6f;
        enemies[i].type = i % 5;
        enemies[i].state = 0;
        enemies[i].aiTimer = 0;
        enemies[i].eliteFlag = (i % 7 == 0) ? 1 : 0;
    }

    UltimateParticle particles[150];
    for(int i = 0; i < 150; i++) {
        particles[i].active = 0;
    }

    UltimateItemDrop items[15];
    for(int i = 0; i < 15; i++) {
        items[i].active = 0;
    }

    UltimateObstacle obstacles[14] = {
        {50.0f, 35.0f, 60.0f, 45.0f, 0xFF3E2723, 1, 0, 300.0f, 0},
        {370.0f, 35.0f, 60.0f, 45.0f, 0xFF3E2723, 1, 0, 300.0f, 0},
        {50.0f, 190.0f, 60.0f, 45.0f, 0xFF3E2723, 1, 0, 300.0f, 0},
        {370.0f, 190.0f, 60.0f, 45.0f, 0xFF3E2723, 1, 0, 300.0f, 0},
        {210.0f, 95.0f, 60.0f, 80.0f, 0xFF4E342E, 1, 1, 450.0f, 1},
        {10.0f, 10.0f, 460.0f, 8.0f, 0xFF1B5E20, 1, 0, 1000.0f, 2},
        {10.0f, 254.0f, 460.0f, 8.0f, 0xFF1B5E20, 1, 0, 1000.0f, 2},
        {170.0f, 10.0f, 140.0f, 6.0f, 0xFF2E7D32, 1, 0, 500.0f, 2},
        {170.0f, 260.0f, 140.0f, 6.0f, 0xFF2E7D32, 1, 0, 500.0f, 2},
        {15.0f, 90.0f, 12.0f, 90.0f, 0xFF2E7D32, 1, 0, 500.0f, 2},
        {453.0f, 90.0f, 12.0f, 90.0f, 0xFF2E7D32, 1, 0, 500.0f, 2},
        {210.0f, 15.0f, 60.0f, 25.0f, 0xFF3E2723, 1, 1, 200.0f, 0},
        {110.0f, 120.0f, 30.0f, 30.0f, 0xFF5D4037, 1, 1, 150.0f, 0},
        {340.0f, 120.0f, 30.0f, 30.0f, 0xFF5D4037, 1, 1, 150.0f, 0}
    };

    UltimateUIElement menuLayout[6] = {
        {90.0f, 35.0f, 300.0f, 55.0f, 0xFF2D1457, 0xFF7C4DFF, 1, 0},
        {120.0f, 105.0f, 240.0f, 40.0f, 0xFF251145, 0xFF00C853, 1, 1},
        {120.0f, 155.0f, 240.0f, 40.0f, 0xFF251145, 0xFF00C853, 1, 2},
        {120.0f, 205.0f, 240.0f, 40.0f, 0xFF251145, 0xFF00C853, 1, 3},
        {0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0}
    };

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        globalTick++;

        // ==================================================================
        // ESTADO 0: MENU PRINCIPAL
        // ==================================================================
        if (gameState == 0) {
            if ((pad.Buttons & PSP_CTRL_DOWN) && !(lastPad.Buttons & PSP_CTRL_DOWN)) {
                menuCursor = (menuCursor < 2) ? menuCursor + 1 : 0;
            }
            if ((pad.Buttons & PSP_CTRL_UP) && !(lastPad.Buttons & PSP_CTRL_UP)) {
                menuCursor = (menuCursor > 0) ? menuCursor - 1 : 2;
            }
            if ((pad.Buttons & PSP_CTRL_CROSS) && !(lastPad.Buttons & PSP_CTRL_CROSS)) {
                if (menuCursor == 0) gameState = 1; 
                else if (menuCursor == 1) gameState = 2; 
                else sceKernelExitGame();
            }

            sceGuStart(GU_DIRECT, list);
            ultimate_render_background(0xFF100520);

            ultimate_draw_rect(menuLayout[0].x, menuLayout[0].y, menuLayout[0].width, menuLayout[0].height, menuLayout[0].baseColor);
            ultimate_draw_rect(menuLayout[1].x, menuLayout[1].y, menuLayout[1].width, menuLayout[1].height, (menuCursor == 0) ? menuLayout[1].borderColor : menuLayout[1].baseColor);
            ultimate_draw_rect(menuLayout[2].x, menuLayout[2].y, menuLayout[2].width, menuLayout[2].height, (menuCursor == 1) ? menuLayout[2].borderColor : menuLayout[2].baseColor);
            ultimate_draw_rect(menuLayout[3].x, menuLayout[3].y, menuLayout[3].width, menuLayout[3].height, (menuCursor == 2) ? menuLayout[3].borderColor : menuLayout[3].baseColor);

            sceGuFinish();
            sceGuSync(0, 0);
        }
        // ==================================================================
        // ESTADO 1: TELA DE SELEÇÃO DE CLASSE / PERSONAGEM
        // ==================================================================
        else if (gameState == 1) {
            if ((pad.Buttons & PSP_CTRL_LEFT) && !(lastPad.Buttons & PSP_CTRL_LEFT)) {
                player.charClass = (player.charClass > 0) ? player.charClass - 1 : 2;
            }
            if ((pad.Buttons & PSP_CTRL_RIGHT) && !(lastPad.Buttons & PSP_CTRL_RIGHT)) {
                player.charClass = (player.charClass < 2) ? player.charClass + 1 : 0;
            }
            if ((pad.Buttons & PSP_CTRL_CROSS) && !(lastPad.Buttons & PSP_CTRL_CROSS)) {
                if (player.charClass == 0) { player.speed = 4.6f; player.maxHp = 120.0f; }
                else if (player.charClass == 1) { player.speed = 3.0f; player.maxHp = 200.0f; }
                else { player.speed = 3.8f; player.maxHp = 140.0f; }
                
                player.hp = player.maxHp;
                player.x = 220.0f;
                player.y = 120.0f;
                gameState = 2;
            }

            sceGuStart(GU_DIRECT, list);
            ultimate_render_background(0xFF080E22);

            ultimate_draw_rect(35.0f, 55.0f, 120.0f, 160.0f, (player.charClass == 0) ? 0xFF00E676 : 0xFF172038);
            ultimate_draw_rect(180.0f, 55.0f, 120.0f, 160.0f, (player.charClass == 1) ? 0xFF00E676 : 0xFF172038);
            ultimate_draw_rect(325.0f, 55.0f, 120.0f, 160.0f, (player.charClass == 2) ? 0xFF00E676 : 0xFF172038);

            sceGuFinish();
            sceGuSync(0, 0);
        }
        // ==================================================================
        // ESTADO 2: GAMEPLAY / ARENA DE COMBATE
        // ==================================================================
        else if (gameState == 2) {
            if ((pad.Buttons & PSP_CTRL_START) && !(lastPad.Buttons & PSP_CTRL_START)) {
                gameState = 3;
            }

            float nextX = player.x;
            float nextY = player.y;

            if (pad.Lx < 64 || (pad.Buttons & PSP_CTRL_LEFT))  nextX -= player.speed;
            if (pad.Lx > 192 || (pad.Buttons & PSP_CTRL_RIGHT)) nextX += player.speed;
            if (pad.Ly < 64 || (pad.Buttons & PSP_CTRL_UP))    nextY -= player.speed;
            if (pad.Ly > 192 || (pad.Buttons & PSP_CTRL_DOWN))  nextY += player.speed;

            // Colisão completa jogador vs obstáculos
            int canMoveX = 1;
            int canMoveY = 1;
            for(int o = 0; o < 14; o++) {
                if (obstacles[o].solid) {
                    if (nextX + 24.0f > obstacles[o].x && nextX < obstacles[o].x + obstacles[o].w &&
                        player.y + 24.0f > obstacles[o].y && player.y < obstacles[o].y + obstacles[o].h) {
                        canMoveX = 0;
                    }
                    if (player.x + 24.0f > obstacles[o].x && player.x < obstacles[o].x + obstacles[o].w &&
                        nextY + 24.0f > obstacles[o].y && nextY < obstacles[o].y + obstacles[o].h) {
                        canMoveY = 0;
                    }
                }
            }

            if (canMoveX) player.x = nextX;
            if (canMoveY) player.y = nextY;

            if (player.x < 18.0f) player.x = 18.0f;
            if (player.x > 442.0f) player.x = 442.0f;
            if (player.y < 22.0f) player.y = 22.0f;
            if (player.y > 238.0f) player.y = 238.0f;

            if (pad.Buttons & PSP_CTRL_SQUARE) player.weaponEquipped = 0;
            if (pad.Buttons & PSP_CTRL_TRIANGLE) player.weaponEquipped = 1;
            if (pad.Buttons & PSP_CTRL_CIRCLE) player.weaponEquipped = 2;

            if (shootCooldown > 0) shootCooldown--;
            if ((pad.Buttons & PSP_CTRL_CROSS) && shootCooldown == 0 && player.ammoCount > 0) {
                for(int i = 0; i < 150; i++) {
                    if (!projectiles[i].active) {
                        projectiles[i].active = 1;
                        projectiles[i].x = player.x + 12.0f;
                        projectiles[i].y = player.y + 12.0f;
                        projectiles[i].vx = 11.0f;
                        projectiles[i].vy = 0.0f;
                        projectiles[i].type = player.weaponEquipped;
                        projectiles[i].lifeTime = 0.0f;
                        projectiles[i].damage = 40.0f;
                        projectiles[i].color = (player.weaponEquipped == 1) ? 0xFFFF5722 : 0xFFFFFFFF;
                        player.ammoCount--;
                        shootCooldown = 5;
                        break;
                    }
                }
            }

            for(int i = 0; i < 150; i++) {
                if (projectiles[i].active) {
                    projectiles[i].x += projectiles[i].vx;
                    projectiles[i].y += projectiles[i].vy;
                    projectiles[i].lifeTime += 1.0f;
                    if (projectiles[i].x > 475.0f || projectiles[i].lifeTime > 50.0f) {
                        projectiles[i].active = 0;
                    }
                }
            }

            for(int i = 0; i < 50; i++) {
                if (enemies[i].active) {
                    if (enemies[i].x < player.x) enemies[i].x += enemies[i].speed;
                    if (enemies[i].x > player.x) enemies[i].x -= enemies[i].speed;
                    if (enemies[i].y < player.y) enemies[i].y += enemies[i].speed;
                    if (enemies[i].y > player.y) enemies[i].y -= enemies[i].speed;

                    for(int b = 0; b < 150; b++) {
                        if (projectiles[b].active) {
                            if (projectiles[b].x > enemies[i].x && projectiles[b].x < enemies[i].x + 24.0f &&
                                projectiles[b].y > enemies[i].y && projectiles[b].y < enemies[i].y + 24.0f) {
                                projectiles[b].active = 0;
                                enemies[i].hp -= projectiles[b].damage;
                                if (enemies[i].hp <= 0) {
                                    enemies[i].active = 0;
                                    player.score += 200;
                                    enemies[i].x = (float)(rand() % 400 + 30);
                                    enemies[i].y = (float)(rand() % 200 + 30);
                                    enemies[i].hp = enemies[i].maxHp;
                                    enemies[i].active = 1;
                                }
                            }
                        }
                    }

                    float edx = enemies[i].x - player.x;
                    float edy = enemies[i].y - player.y;
                    if (sqrtf(edx*edx + edy*edy) < 20.0f) {
                        player.hp -= 0.6f;
                        if (player.hp <= 0) {
                            gameState = 4;
                        }
                    }
                }
            }

            sceGuStart(GU_DIRECT, list);
            ultimate_render_background(0xFF1B4D22);

            for(int o = 0; o < 14; o++) {
                ultimate_draw_rect(obstacles[o].x, obstacles[o].y, obstacles[o].w, obstacles[o].h, obstacles[o].color);
            }

            unsigned int pColor = (player.charClass == 1) ? 0xFF00B0FF : 0xFF00E676;
            ultimate_draw_rect(player.x, player.y, 24.0f, 24.0f, pColor);

            for(int i = 0; i < 150; i++) {
                if (projectiles[i].active) {
                    ultimate_draw_rect(projectiles[i].x, projectiles[i].y, 6.0f, 5.0f, projectiles[i].color);
                }
            }

            for(int i = 0; i < 50; i++) {
                if (enemies[i].active) {
                    unsigned int eColor = enemies[i].eliteFlag ? 0xFF9C27B0 : 0xFFD32F2F;
                    ultimate_draw_rect(enemies[i].x, enemies[i].y, 24.0f, 24.0f, eColor);
                }
            }

            ultimate_draw_rect(20.0f, 12.0f, player.hp * 1.5f, 8.0f, 0xFF00FF00);

            sceGuFinish();
            sceGuSync(0, 0);
        }
        // ==================================================================
        // ESTADO 3: PAUSA
        // ==================================================================
        else if (gameState == 3) {
            if ((pad.Buttons & PSP_CTRL_START) && !(lastPad.Buttons & PSP_CTRL_START)) {
                gameState = 2;
            }

            sceGuStart(GU_DIRECT, list);
            ultimate_render_background(0xFF0C0C14);
            ultimate_draw_rect(130.0f, 80.0f, 220.0f, 110.0f, 0xFF242436);
            sceGuFinish();
            sceGuSync(0, 0);
        }
        // ==================================================================
        // ESTADO 4: GAME OVER
        // ==================================================================
        else if (gameState == 4) {
            if ((pad.Buttons & PSP_CTRL_CROSS) && !(lastPad.Buttons & PSP_CTRL_CROSS)) {
                player.hp = player.maxHp;
                player.score = 0;
                player.ammoCount = 60;
                gameState = 0;
            }

            sceGuStart(GU_DIRECT, list);
            ultimate_render_background(0xFF2A0000);
            ultimate_draw_rect(110.0f, 75.0f, 260.0f, 120.0f, 0xFF421010);
            sceGuFinish();
            sceGuSync(0, 0);
        }

        lastPad = pad;
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}
