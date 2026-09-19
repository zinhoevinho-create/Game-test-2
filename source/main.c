#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdlib.h>

PSP_MODULE_INFO("Zooba 2 PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}

// Estrutura para os vértices dos gráficos na tela do PSP
typedef struct {
    unsigned int color;
    short x, y, z;
} Vertex;

void draw_quad(short x, short y, short w, short h, unsigned int color) {
    Vertex* vertices = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
    if (!vertices) return;

    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0;

    vertices[1].color = color;
    vertices[1].x = x + w;
    vertices[1].y = y + h;
    vertices[1].z = 0;

    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
}

typedef struct {
    int x, y;
    int speedX, speedY;
    int ativo;
} Inimigo;

int main(void) {
    setup_callbacks();

    // Inicialização da Gu (Gráficos do PSP)
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(480, 272, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
    sceGuViewport(2048, 2048, 480, 272);
    sceGuDepthRange(0xcf9c, 0x50);
    sceGuScissor(0, 0, 480, 272);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Jogador
    int playerX = 220;
    int playerY = 120;
    int velocidade = 3;

    // Inimigos do Zooba 2
    Inimigo inimigos[3] = {
        {80, 50, 2, 1, 1},
        {350, 60, -2, 2, 1},
        {200, 200, 1, -2, 1}
    };

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        // Movimentação do Jogador
        if (pad.Lx < 64 || (pad.Buttons & PSP_CTRL_LEFT))  playerX -= velocidade;
        if (pad.Lx > 192 || (pad.Buttons & PSP_CTRL_RIGHT)) playerX += velocidade;
        if (pad.Ly < 64 || (pad.Buttons & PSP_CTRL_UP))    playerY -= velocidade;
        if (pad.Ly > 192 || (pad.Buttons & PSP_CTRL_DOWN))  playerY += velocidade;

        // Limites da tela
        if (playerX < 10) playerX = 10;
        if (playerX > 450) playerX = 450;
        if (playerY < 10) playerY = 10;
        if (playerY > 240) playerY = 240;

        // Atualiza IA dos Inimigos
        for (int i = 0; i < 3; i++) {
            if (inimigos[i].ativo) {
                inimigos[i].x += inimigos[i].speedX;
                inimigos[i].y += inimigos[i].speedY;

                if (inimigos[i].x <= 10 || inimigos[i].x >= 450) inimigos[i].speedX *= -1;
                if (inimigos[i].y <= 10 || inimigos[i].y >= 240) inimigos[i].speedY *= -1;
            }
        }

        // Renderização gráfica na tela
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF222222); // Cor de fundo (Cinza escuro estilo arena)
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        // Desenha o Jogador (Cor Verde)
        unsigned int corJogador = 0xFF00FF00;
        if (pad.Buttons & PSP_CTRL_SQUARE)   corJogador = 0xFFFF0000; // Vermelho (Shotgun)
        else if (pad.Buttons & PSP_CTRL_TRIANGLE) corJogador = 0xFF00FFFF; // Amarelo/Ciano (Lança)
        else if (pad.Buttons & PSP_CTRL_CROSS)    corJogador = 0xFFFF00FF; // Rosa (Bomba)
        else if (pad.Buttons & PSP_CTRL_CIRCLE)   corJogador = 0xFFFFFFFF; // Branco (Kit Médico)

        draw_quad(playerX, playerY, 20, 20, corJogador);

        // Desenha os Inimigos (Cor Azul)
        for (int i = 0; i < 3; i++) {
            if (inimigos[i].ativo) {
                draw_quad(inimigos[i].x, inimigos[i].y, 20, 20, 0xFFFF5555);
            }
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}
