#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
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

typedef struct {
    float x, y;
    float w, h;
    unsigned int color;
} Rect;

void draw_rect(float x, float y, float w, float h, unsigned int color) {
    // Alinha diretamente para coordenadas de tela 2D simples
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

int main(void) {
    setup_callbacks();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(480, 272, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    
    sceGuOffset(0, 0);
    sceGuViewport(0, 0, 480, 272);
    sceGuDepthRange(0xcf9c, 0x50);
    
    sceGuScissor(0, 0, 480, 272);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    float playerX = 220.0f;
    float playerY = 120.0f;
    float velocidade = 3.5f;

    float enX[3] = {80.0f, 350.0f, 200.0f};
    float enY[3] = {50.0f, 180.0f, 100.0f};
    float enSpdX[3] = {2.0f, -2.0f, 1.5f};
    float enSpdY[3] = {1.0f, 1.5f, -2.0f};

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        // Movimentação do Jogador
        if (pad.Lx < 64 || (pad.Buttons & PSP_CTRL_LEFT))  playerX -= velocidade;
        if (pad.Lx > 192 || (pad.Buttons & PSP_CTRL_RIGHT)) playerX += velocidade;
        if (pad.Ly < 64 || (pad.Buttons & PSP_CTRL_UP))    playerY -= velocidade;
        if (pad.Ly > 192 || (pad.Buttons & PSP_CTRL_DOWN))  playerY += velocidade;

        // Limites de tela (480x272)
        if (playerX < 0.0f) playerX = 0.0f;
        if (playerX > 450.0f) playerX = 450.0f;
        if (playerY < 0.0f) playerY = 0.0f;
        if (playerY > 242.0f) playerY = 242.0f;

        // Atualiza Inimigos
        for (int i = 0; i < 3; i++) {
            enX[i] += enSpdX[i];
            enY[i] += enSpdY[i];

            if (enX[i] <= 0.0f || enX[i] >= 450.0f) enSpdX[i] *= -1.0f;
            if (enY[i] <= 0.0f || enY[i] >= 242.0f) enSpdY[i] *= -1.0f;
        }

        // Renderização limpa
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF332211); // Cor de fundo azulada/marrom para ver que renderizou
        sceGuClear(GU_COLOR_BUFFER_BIT);

        // Cor do Jogador baseada nos botões
        unsigned int corJogador = 0xFF00FF00; // Verde padrão
        if (pad.Buttons & PSP_CTRL_SQUARE)   corJogador = 0xFF0000FF; // Vermelho
        else if (pad.Buttons & PSP_CTRL_TRIANGLE) corJogador = 0xFF00FFFF; // Amarelo
        else if (pad.Buttons & PSP_CTRL_CROSS)    corJogador = 0xFFFF00FF; // Rosa

        // Desenha Jogador
        draw_rect(playerX, playerY, 20.0f, 20.0f, corJogador);

        // Desenha Inimigos
        for (int i = 0; i < 3; i++) {
            draw_rect(enX[i], enY[i], 20.0f, 20.0f, 0xFF5555FF);
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}
