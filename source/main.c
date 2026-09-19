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

void draw_rect(float x, float y, float w, float h, unsigned int color) {
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

    int estadoJogo = 0; // 0 = Menu Principal, 1 = Em Jogo

    float playerX = 220.0f;
    float playerY = 120.0f;
    float velocidade = 3.0f;

    // Inimigos do Zooba
    float enX[3] = {80.0f, 350.0f, 200.0f};
    float enY[3] = {50.0f, 180.0f, 100.0f};
    float enSpdX[3] = {1.5f, -1.5f, 1.0f};
    float enSpdY[3] = {1.0f, 1.2f, -1.5f};

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (estadoJogo == 0) {
            // Tela de Menu Principal
            if (pad.Buttons & PSP_CTRL_START) {
                estadoJogo = 1; // Inicia a partida ao apertar START
            }

            sceGuStart(GU_DIRECT, list);
            sceGuClearColor(0xFF111122); // Fundo azul escuro de menu
            sceGuClear(GU_COLOR_BUFFER_BIT);

            // Desenha um painel central imitando o Menu do Zooba 2
            draw_rect(100.0f, 70.0f, 280.0f, 130.0f, 0xFF333355);
            draw_rect(140.0f, 130.0f, 200.0f, 40.0f, 0xFF00AA00); // Botao Jogar

            sceGuFinish();
            sceGuSync(0, 0);
        } 
        else if (estadoJogo == 1) {
            // Movimentação do Jogador
            if (pad.Lx < 64 || (pad.Buttons & PSP_CTRL_LEFT))  playerX -= velocidade;
            if (pad.Lx > 192 || (pad.Buttons & PSP_CTRL_RIGHT)) playerX += velocidade;
            if (pad.Ly < 64 || (pad.Buttons & PSP_CTRL_UP))    playerY -= velocidade;
            if (pad.Ly > 192 || (pad.Buttons & PSP_CTRL_DOWN))  playerY += velocidade;

            // Limites da Arena do Mapa
            if (playerX < 20.0f) playerX = 20.0f;
            if (playerX > 440.0f) playerX = 440.0f;
            if (playerY < 20.0f) playerY = 20.0f;
            if (playerY > 230.0f) playerY = 230.0f;

            // Atualiza Inimigos
            for (int i = 0; i < 3; i++) {
                enX[i] += enSpdX[i];
                enY[i] += enSpdY[i];

                if (enX[i] <= 20.0f || enX[i] >= 440.0f) enSpdX[i] *= -1.0f;
                if (enY[i] <= 20.0f || enY[i] >= 230.0f) enSpdY[i] *= -1.0f;
            }

            // Renderização da Arena de Batalha
            sceGuStart(GU_DIRECT, list);
            sceGuClearColor(0xFF224422); // Fundo verde floresta/arena do Zooba
            sceGuClear(GU_COLOR_BUFFER_BIT);

            // Paredes e Obstáculos do Mapa (Bordas e Caixas)
            draw_rect(10.0f, 10.0f, 460.0f, 10.0f, 0xFF112211); // Borda Superior
            draw_rect(10.0f, 252.0f, 460.0f, 10.0f, 0xFF112211); // Borda Inferior
            draw_rect(200.0f, 100.0f, 80.0f, 60.0f, 0xFF445566); // Obstáculo Central no Mapa

            // Cor do Personagem e Armas baseada nos botões apertados
            unsigned int corJogador = 0xFF00FF00; // Personagem padrão (Verde)
            if (pad.Buttons & PSP_CTRL_SQUARE) {
                corJogador = 0xFF0000FF; // Vermelho (Atirando com Espingarda)
            } else if (pad.Buttons & PSP_CTRL_TRIANGLE) {
                corJogador = 0xFF00FFFF; // Amarelo/Ciano (Ataque com Lança)
            } else if (pad.Buttons & PSP_CTRL_CROSS) {
                corJogador = 0xFFFF00FF; // Rosa (Jogando Bomba)
            } else if (pad.Buttons & PSP_CTRL_CIRCLE) {
                corJogador = 0xFFFFFFFF; // Branco (Usando Kit Médico)
            }

            // Desenha o Jogador (Animal)
            draw_rect(playerX, playerY, 24.0f, 24.0f, corJogador);

            // Desenha os Inimigos na Arena
            for (int i = 0; i < 3; i++) {
                draw_rect(enX[i], enY[i], 24.0f, 24.0f, 0xFF0000AA); // Inimigos em Azul escuro
            }

            sceGuFinish();
            sceGuSync(0, 0);
        }

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}

