#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>

PSP_MODULE_INFO("Zooba 2 PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

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
    int x, y;
    int speedX, speedY;
    int ativo;
} Inimigo;

int main(void) {
    setup_callbacks();
    pspDebugScreenInit();

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    int playerX = 240;
    int playerY = 136;
    int velocidade = 3;

    Inimigo inimigos[3] = {
        {100, 50, 1, 1, 1},
        {380, 80, -1, 1, 1},
        {240, 220, 1, -1, 1}
    };

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Lx < 64 || (pad.Buttons & PSP_CTRL_LEFT))  playerX -= velocidade;
        if (pad.Lx > 192 || (pad.Buttons & PSP_CTRL_RIGHT)) playerX += velocidade;
        if (pad.Ly < 64 || (pad.Buttons & PSP_CTRL_UP))    playerY -= velocidade;
        if (pad.Ly > 192 || (pad.Buttons & PSP_CTRL_DOWN))  playerY += velocidade;

        if (playerX < 10) playerX = 10;
        if (playerX > 460) playerX = 460;
        if (playerY < 30) playerY = 30;
        if (playerY > 250) playerY = 250;

        for (int i = 0; i < 3; i++) {
            if (inimigos[i].ativo) {
                inimigos[i].x += inimigos[i].speedX;
                inimigos[i].y += inimigos[i].speedY;

                if (inimigos[i].x <= 15 || inimigos[i].x >= 460) inimigos[i].speedX *= -1;
                if (inimigos[i].y <= 35 || inimigos[i].y >= 250) inimigos[i].speedY *= -1;
            }
        }

        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("=== ZOOBA 2 - PSP EDITION ===\n");
        pspDebugScreenPrintf("Jogador -> X: %d | Y: %d\n", playerX, playerY);
        
        pspDebugScreenPrintf("INIMIGOS EM CAMPO:\n");
        for (int i = 0; i < 3; i++) {
            pspDebugScreenPrintf(" - Inimigo %d [X: %d, Y: %d]\n", i + 1, inimigos[i].x, inimigos[i].y);
        }

        pspDebugScreenPrintf("\nCONTROLES: Analogico/D-Pad (Mover)\n");

        if (pad.Buttons & PSP_CTRL_SQUARE) {
            pspDebugScreenPrintf("[!] ACAO: Atirando com a Shotgun!      \n");
        } else if (pad.Buttons & PSP_CTRL_TRIANGLE) {
            pspDebugScreenPrintf("[!] ACAO: Atacando com a Lanca!        \n");
        } else if (pad.Buttons & PSP_CTRL_CROSS) {
            pspDebugScreenPrintf("[!] ACAO: Arremessando Bomba!          \n");
        } else if (pad.Buttons & PSP_CTRL_CIRCLE) {
            pspDebugScreenPrintf("[!] ACAO: Usando Kit Medico!           \n");
        } else {
            pspDebugScreenPrintf("                                       \n");
        }

        sceDisplayWaitVblankStart();
    }

    return 0;
}

