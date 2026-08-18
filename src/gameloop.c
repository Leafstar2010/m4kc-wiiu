#include <limits.h>
#include "gameloop.h"
#include "textures.h"
#include "utility.h"
#include "options.h"
#include "blocks.h"
#include "menus.h"
#include "data.h"
#include "gui.h"
#include <vpad/input.h>
#include <coreinit/thread.h>

World world = { 0 };
Player *player = &world.player;

int gameState = STATE_TITLE;
int gamePopup;

static int guiOn;
static int debugOn;

static SDL_Rect backgroundRect;
static char *errorMessage = NULL;
static long l;

static void gameLoop_gameplay        (SDL_Renderer *, Inputs *);
static void gameLoop_drawPopup       (SDL_Renderer *, Inputs *);
static void gameLoop_processMovement (Inputs *, int);

static uint32_t
        fps_lastmil = 0,
        fps_count   = 0,
        fps_now     = 0;

/* ===================== MULTITHREADING DATA ===================== */

#define RENDER_THREAD_STACK_SIZE 0x8000

static uint32_t pixelBuffer[214 * 120];

static OSThread renderThread1;
static OSThread renderThread2;

// Per-thread raycasting state
typedef struct {
        double
                f21, f22, f23, f24, f25, f26,
                f27, f28, f29, f30, f31, f32,
                f33, f34, f35, f36;
        int selectedPass;
        IntCoords coordPass;
        IntCoords blockSelectOffset;
        IntCoords blockRayPosition;
        IntCoords lookup_ago;
        IntCoords lookup_now;
        Chunk *chunk;
        int startY;
        int endY;
        int headInWater;
        int effectDrawDistance;
        double effectFov;
        int blockSelectedPrev;
        IntCoords blockSelectPrev;
} RenderThreadData;

static RenderThreadData threadData1;
static RenderThreadData threadData2;
static RenderThreadData mainThreadData;

/* raycastRows
 * Renders a range of pixel rows for one thread.
 */
static void raycastRows (RenderThreadData *rtd) {
        double effectFov = rtd->effectFov;
        int headInWater = rtd->headInWater;
        double rayDistanceLimit;
        
        for (int pixelX = 0; pixelX < BUFFER_W; pixelX++) {
                double rayOffsetX = (pixelX - BUFFER_HALF_W) / effectFov;
                for (int pixelY = rtd->startY; pixelY < rtd->endY; pixelY++) {
                        int finalPixelColor = 0;
                        int pixelMist = 255;
                        int pixelShade = 0;
                        
                        double rayOffsetY = (pixelY - BUFFER_HALF_H) / effectFov;

                        rtd->f21 = 1.0;
                        
                        rtd->f22 = rtd->f21 * player->vectorV.y + rayOffsetY * player->vectorV.x;
                        rtd->f23 = rayOffsetY * player->vectorV.y - rtd->f21 * player->vectorV.x;
                        rtd->f24 = rayOffsetX * player->vectorH.y + rtd->f22 * player->vectorH.x;
                        rtd->f25 = rtd->f22 * player->vectorH.y - rayOffsetX * player->vectorH.x;

                        rayDistanceLimit = (double)rtd->effectDrawDistance;
                        
                        rtd->f26 = 5.0;
                        for (int blockFace = 0; blockFace < 3; blockFace++) {
                                rtd->f27 = rtd->f24;
                                if (blockFace == 1) rtd->f27 = rtd->f23;
                                if (blockFace == 2) rtd->f27 = rtd->f25;
                                rtd->f28 = 1.0 / ((rtd->f27 < 0.0) ? (-1.0 * rtd->f27) : rtd->f27);
                                rtd->f29 = rtd->f24 * rtd->f28;
                                rtd->f30 = rtd->f23 * rtd->f28;
                                rtd->f31 = rtd->f25 * rtd->f28;
                                rtd->f32 = player->pos.x - floor(player->pos.x);
                                if (blockFace == 1) rtd->f32 = player->pos.y - floor(player->pos.y);
                                if (blockFace == 2) rtd->f32 = player->pos.z - floor(player->pos.z);
                                if (rtd->f27 > 0.0) rtd->f32 = 1.0 - rtd->f32;
                                rtd->f33 = rtd->f28 * rtd->f32;
                                rtd->f34 = player->pos.x + rtd->f29 * rtd->f32;
                                rtd->f35 = player->pos.y + rtd->f30 * rtd->f32;
                                rtd->f36 = player->pos.z + rtd->f31 * rtd->f32;
                                if (rtd->f27 < 0.0) {
                                        if (blockFace == 0) rtd->f34--;
                                        if (blockFace == 1) rtd->f35--;
                                        if (blockFace == 2) rtd->f36--;
                                }
                                
                                while (rtd->f33 < rayDistanceLimit) {
                                        rtd->blockRayPosition.x = (int)floor(rtd->f34);
                                        rtd->blockRayPosition.y = (int)floor(rtd->f35);
                                        rtd->blockRayPosition.z = (int)floor(rtd->f36);
                                        
                                        rtd->lookup_now.x = rtd->blockRayPosition.x >> 6;
                                        rtd->lookup_now.y = rtd->blockRayPosition.y >> 6;
                                        rtd->lookup_now.z = rtd->blockRayPosition.z >> 6;

                                        if (
                                                rtd->lookup_now.x != rtd->lookup_ago.x ||
                                                rtd->lookup_now.y != rtd->lookup_ago.y ||
                                                rtd->lookup_now.z != rtd->lookup_ago.z
                                        ) {
                                                rtd->lookup_ago = rtd->lookup_now;
                                                rtd->lookup_now.x &= 0x3FF;
                                                rtd->lookup_now.y &= 0x3FF;
                                                rtd->lookup_now.z &= 0x3FF;
                                                rtd->lookup_now.y <<= 10;
                                                rtd->lookup_now.z <<= 20;
                                                
                                                uint32_t lookup_hash = rtd->lookup_now.x | rtd->lookup_now.y | rtd->lookup_now.z;
                                                lookup_hash++;
                                                
                                                int lookup_first = 0;
                                                int lookup_last = CHUNKARR_SIZE - 1;
                                                int lookup_middle = (CHUNKARR_SIZE - 1) / 2;

                                                while (lookup_first <= lookup_last) {
                                                        if (world.chunk[lookup_middle].coordHash > lookup_hash) {
                                                                lookup_first = lookup_middle + 1;
                                                        } else if (world.chunk[lookup_middle].coordHash == lookup_hash) {
                                                                rtd->chunk = &world.chunk[lookup_middle];
                                                                goto foundChunk;
                                                        } else {
                                                                lookup_last = lookup_middle - 1;
                                                        }
                                                        lookup_middle = (lookup_first + lookup_last) / 2;
                                                }
                                                rtd->chunk = NULL;
                                        }
                                        
                                        Block intersectedBlock;
                                        foundChunk: if (rtd->chunk) {
                                                intersectedBlock = rtd->chunk->blocks[
                                                        nmod(rtd->blockRayPosition.x, 64) +
                                                        (nmod(rtd->blockRayPosition.y, 64) << 6) +
                                                        (nmod(rtd->blockRayPosition.z, 64) << 12)
                                                ];
                                        } else {
                                                intersectedBlock = 0;
                                                goto chunkNull;
                                        }
                                        
                                        if (
                                                intersectedBlock != BLOCK_AIR &&
                                                !(headInWater && intersectedBlock == BLOCK_WATER)
                                        ) {
                                                int textureX = (int)floor((rtd->f34 + rtd->f36) * 16.0) & 0xF;
                                                int textureY = ((int)floor(rtd->f35 * 16.0) & 0xF) + 16;
                                                if (blockFace == 1) {
                                                        textureX = (int)floor(rtd->f34 * 16.0) & 0xF;
                                                        textureY = (int)floor(rtd->f36 * 16.0) & 0xF;
                                                        if (intersectedBlock == BLOCK_LEAVES) {
                                                                textureY &= 0x7;
                                                                textureY += 32;
                                                        } else {
                                                                if (rtd->f30 < 0.0) textureY += 32;
                                                        }
                                                }

                                                int pixelColor = 0xFFFFFF;
                                                if (
                                                        (
                                                                rtd->blockSelectedPrev == 0 ||
                                                                rtd->blockRayPosition.x != rtd->blockSelectPrev.x ||
                                                                rtd->blockRayPosition.y != rtd->blockSelectPrev.y ||
                                                                rtd->blockRayPosition.z != rtd->blockSelectPrev.z
                                                        ) || (
                                                                textureX > 0 &&
                                                                textureY % 16 > 0 &&
                                                                textureX < 15 &&
                                                                textureY % 16 < 15
                                                        ) || !guiOn || gamePopup
                                                ) {
                                                        if (intersectedBlock >= NUMBER_OF_BLOCKS) {
                                                                pixelColor = 0xFF0000;
                                                        } else {
                                                                pixelColor = textures[
                                                                        textureX + (textureY * 16) + intersectedBlock * 256 * 3
                                                                ];
                                                        }
                                                }
                                                
                                                if (
                                                        rtd->f33 < rtd->f26 &&
                                                        pixelX == BUFFER_HALF_W &&
                                                        pixelY == BUFFER_HALF_H
                                                ) {
                                                        rtd->selectedPass = 1;
                                                        rtd->coordPass = rtd->blockRayPosition;
                                                        
                                                        rtd->blockSelectOffset = (const IntCoords) { 0 };
                                                        switch (blockFace) {
                                                                case 0: rtd->blockSelectOffset.x = 1 - 2 * (rtd->f27 > 0.0); break;
                                                                case 1: rtd->blockSelectOffset.y = 1 - 2 * (rtd->f27 > 0.0); break;
                                                                case 2: rtd->blockSelectOffset.z = 1 - 2 * (rtd->f27 > 0.0); break;
                                                        }
                                                        rtd->f26 = rtd->f33;
                                                }
                                                
                                                if (pixelColor > 0) {
                                                        finalPixelColor = pixelColor;
                                                        pixelMist = 255 - (int)(rtd->f33 / (double)rtd->effectDrawDistance * 255.0);
                                                        pixelShade = 255 - (blockFace + 2) % 3 * 50;
                                                        rayDistanceLimit = rtd->f33;
                                                }
                                        }
                                        chunkNull:
                                        rtd->f34 += rtd->f29;
                                        rtd->f35 += rtd->f30;
                                        rtd->f36 += rtd->f31;
                                        rtd->f33 += rtd->f28;
                                }
                        }
                        
                        // Crosshair
                        if (options.trapMouse && (
                                (pixelX == BUFFER_HALF_W && abs(BUFFER_HALF_H - pixelY) < 4) ||
                                (pixelY == BUFFER_HALF_H && abs(BUFFER_HALF_W - pixelX) < 4)
                        )) {
                                finalPixelColor = 0x1000000 - finalPixelColor;
                        }
                        
                        // Pack into pixel buffer
                        if (finalPixelColor > 0) {
                                int r = ((finalPixelColor >> 16 & 0xFF) * pixelShade) >> 8;
                                int g = ((finalPixelColor >> 8 & 0xFF) * pixelShade) >> 8;
                                int b = ((finalPixelColor & 0xFF) * pixelShade) >> 8;
                                int a = options.fogType ? (int)(sqrt(pixelMist) * 16) : pixelMist;
                                pixelBuffer[pixelY * BUFFER_W + pixelX] = (a << 24) | (r << 16) | (g << 8) | b;
                        } else {
                                pixelBuffer[pixelY * BUFFER_W + pixelX] = 0;
                        }
                }
        }
}

/* renderThread1Entry */
static int renderThread1Entry (int argc, const char **argv) {
        (void)(argc);
        (void)(argv);
        raycastRows(&threadData1);
        return 0;
}

/* renderThread2Entry */
static int renderThread2Entry (int argc, const char **argv) {
        (void)(argc);
        (void)(argv);
        raycastRows(&threadData2);
        return 0;
}

/* ===================== GAME LOOP ===================== */

void gameLoop_resetGame () {
        l = SDL_GetTicks();

        gamePopup = 0;
        guiOn   = 1;
        debugOn = 0;

        backgroundRect.x = 0;
        backgroundRect.y = 0;
        backgroundRect.w = BUFFER_W;
        backgroundRect.h = BUFFER_H;
        
        options.trapMouse = 1;
        chatAdd("Game started");
}

int gameLoop (Inputs *inputs, SDL_Renderer *renderer) {
        if (errorMessage) {
                dirtBg(renderer);
                SDL_SetRenderDrawColor(renderer, 255, 128, 128, 255);
                centerStr(renderer, "Error:", BUFFER_HALF_W, BUFFER_HALF_H - 20);
                white(renderer);
                centerStr(renderer, errorMessage, BUFFER_HALF_W, BUFFER_HALF_H - 4);
                SDL_RenderPresent(renderer);
                return 1;
        }

        switch (gameState) {
        case STATE_TITLE:
                if (state_title(renderer, inputs, &gameState)) return 0;
                break;
        case STATE_SELECT_WORLD:
                state_selectWorld(renderer, inputs, &gameState, &world);
                break;
        case STATE_NEW_WORLD:
                state_newWorld(renderer, inputs, &gameState, &world);
                break;
        case STATE_LOADING:
                if (state_loading(renderer, &world, world.seed, player->pos)) {
                        gameLoop_resetGame();
                        gameState = STATE_GAMEPLAY;
                }
                break;
        case STATE_GAMEPLAY:
                gameLoop_gameplay(renderer, inputs);
                break;
        case STATE_OPTIONS:
                state_options(renderer, inputs, &gameState);
                break;
        default:
                state_egg(renderer, inputs, &gameState);
                break;
        }

        if (gameState != STATE_GAMEPLAY || gamePopup != POPUP_HUD) {
                inputs->mouse.left  = 0;
                inputs->mouse.right = 0;
        }

        return 1;
}

static void gameLoop_gameplay (SDL_Renderer *renderer, Inputs *inputs) {
        static int blockSelected = 0;
        static IntCoords blockSelect = { 0 };
        static IntCoords blockSelectOffset = { 0 };
        static SDL_Texture *frameTexture = NULL;
        static uint32_t lastBreakTime = 0;

        // Read VPAD input
        {
                VPADStatus vpad;
                VPADReadError vpadErr;
                if (VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadErr) > 0 && vpadErr == VPAD_READ_SUCCESS) {
                        inputs->keyboard.w = (vpad.leftStick.y > 0.3f);
                        inputs->keyboard.s = (vpad.leftStick.y < -0.3f);
                        inputs->keyboard.a = (vpad.leftStick.x < -0.3f);
                        inputs->keyboard.d = (vpad.leftStick.x > 0.3f);

                        inputs->mouse.x = (int)(vpad.rightStick.x * 15.0f);
                        inputs->mouse.y = (int)(vpad.rightStick.y * -8.0f);

                        inputs->keyboard.space = (vpad.hold & VPAD_BUTTON_A) ? 1 : 0;
                        
                        if (vpad.hold & VPAD_BUTTON_ZR) inputs->mouse.left = 1;
                        if (vpad.hold & VPAD_BUTTON_ZL) inputs->mouse.right = 1;
                        
                        if (vpad.trigger & VPAD_BUTTON_PLUS) inputs->keyboard.esc = 1;
                        if (vpad.trigger & VPAD_BUTTON_Y) inputs->keyboard.e = 1;
                        if (vpad.trigger & VPAD_BUTTON_X) inputs->keyboard.f1 = 1;
                        if (vpad.trigger & VPAD_BUTTON_MINUS) inputs->keyboard.f3 = 1;
                        
                        if (vpad.trigger & VPAD_BUTTON_LEFT || vpad.trigger & VPAD_BUTTON_L) {
                                player->inventory.hotbarSelect--;
                                player->inventory.hotbarSelect = nmod(player->inventory.hotbarSelect, 9);
                        }
                        if (vpad.trigger & VPAD_BUTTON_RIGHT || vpad.trigger & VPAD_BUTTON_R) {
                                player->inventory.hotbarSelect++;
                                player->inventory.hotbarSelect = nmod(player->inventory.hotbarSelect, 9);
                        }
                }
        }

        int headInWater = World_getBlock(&world, player->pos.x, player->pos.y, player->pos.z) == BLOCK_WATER;
        int feetInWater = World_getBlock(&world, player->pos.x, player->pos.y + 1, player->pos.z) == BLOCK_WATER;

        int effectDrawDistance = options.drawDistance;
        if (headInWater) { effectDrawDistance = 10; }

        player->vectorH.x = sin(player->hRot);
        player->vectorH.y = cos(player->hRot);
        player->vectorV.x = sin(player->vRot);
        player->vectorV.y = cos(player->vRot);

        double timeCoef;
        switch (world.dayNightMode) {
        case 0:
                timeCoef  = (double)(world.time % 102944) / 16384.0;
                timeCoef  = sin(timeCoef);
                timeCoef /= sqrt(timeCoef * timeCoef + (1.0 / 128.0));
                timeCoef  = (timeCoef + 1.0) / 2.0;
                break;
        case 1: timeCoef = 1.0; break;
        case 2: timeCoef = 0.0; break;
        }

        // Sky color
        int skyR, skyG, skyB;
        if (headInWater) {
                skyR = 48 * timeCoef;
                skyG = 96 * timeCoef;
                skyB = 200 * timeCoef;
        } else {
                skyR = 153 * timeCoef;
                skyG = 204 * timeCoef;
                skyB = 255 * timeCoef;
        }

        SDL_SetRenderDrawColor(renderer, skyR, skyG, skyB, 255);
        SDL_RenderClear(renderer);

        if (inputs->keyboard.esc) {
                gamePopup = gamePopup ? 0 : 1;
                inputs->keyboard.esc = 0;
        }

        fps_count++;
        if (fps_lastmil < SDL_GetTicks() - 1000) {
                fps_lastmil = SDL_GetTicks();
                fps_now = fps_count;
                fps_count = 0;
        }

        while (SDL_GetTicks() - l > 10L) {
                world.time++;
                l += 10L;
                gameLoop_processMovement(inputs, feetInWater);
        }

        // Prepare thread data
        double effectFov = options.fov;
        if (headInWater) { effectFov += 20.0; }

        // Thread 1: rows 0-44
        threadData1.startY = 0;
        threadData1.endY = 45;
        threadData1.headInWater = headInWater;
        threadData1.effectDrawDistance = effectDrawDistance;
        threadData1.effectFov = effectFov;
        threadData1.blockSelectedPrev = blockSelected;
        threadData1.blockSelectPrev = blockSelect;
        threadData1.lookup_ago = (const IntCoords) { 100000000, 100000000, 100000000 };
        threadData1.lookup_now = (const IntCoords) { 0, 0, 0 };
        threadData1.selectedPass = 0;
        threadData1.f26 = 5.0;

        // Thread 2: rows 45-89
        threadData2.startY = 45;
        threadData2.endY = 90;
        threadData2.headInWater = headInWater;
        threadData2.effectDrawDistance = effectDrawDistance;
        threadData2.effectFov = effectFov;
        threadData2.blockSelectedPrev = blockSelected;
        threadData2.blockSelectPrev = blockSelect;
        threadData2.lookup_ago = (const IntCoords) { 100000000, 100000000, 100000000 };
        threadData2.lookup_now = (const IntCoords) { 0, 0, 0 };
        threadData2.selectedPass = 0;
        threadData2.f26 = 5.0;

        // Main thread: rows 90-119
        mainThreadData.startY = 90;
        mainThreadData.endY = 120;
        mainThreadData.headInWater = headInWater;
        mainThreadData.effectDrawDistance = effectDrawDistance;
        mainThreadData.effectFov = effectFov;
        mainThreadData.blockSelectedPrev = blockSelected;
        mainThreadData.blockSelectPrev = blockSelect;
        mainThreadData.lookup_ago = (const IntCoords) { 100000000, 100000000, 100000000 };
        mainThreadData.lookup_now = (const IntCoords) { 0, 0, 0 };
        mainThreadData.selectedPass = 0;
        mainThreadData.f26 = 5.0;

        // Spawn both render threads first
        OSCreateThread(&renderThread1, renderThread1Entry, 0, NULL,
                (void*)((uint8_t*)&threadData1 + RENDER_THREAD_STACK_SIZE), RENDER_THREAD_STACK_SIZE, 16, 0);
        OSCreateThread(&renderThread2, renderThread2Entry, 0, NULL,
                (void*)((uint8_t*)&threadData2 + RENDER_THREAD_STACK_SIZE), RENDER_THREAD_STACK_SIZE, 17, 0);

        OSResumeThread(&renderThread1);
        OSResumeThread(&renderThread2);

        // Main thread renders its portion while the other threads work
        raycastRows(&mainThreadData);

        // Wait for both threads to finish
        OSJoinThread(&renderThread1, NULL);
        OSJoinThread(&renderThread2, NULL);

        // Create the frame texture once with blend mode for fog
        if (frameTexture == NULL) {
                frameTexture = SDL_CreateTexture(renderer,
                        SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING,
                        BUFFER_W, BUFFER_H);
                SDL_SetTextureBlendMode(frameTexture, SDL_BLENDMODE_BLEND);
        }

        SDL_UpdateTexture(frameTexture, NULL, pixelBuffer, BUFFER_W * 4);
        SDL_RenderCopy(renderer, frameTexture, NULL, NULL);

        // Combine block selection from all three renderers
        if (threadData1.selectedPass) {
                blockSelected = 1;
                blockSelect = threadData1.coordPass;
                blockSelectOffset = threadData1.blockSelectOffset;
        } else if (threadData2.selectedPass) {
                blockSelected = 1;
                blockSelect = threadData2.coordPass;
                blockSelectOffset = threadData2.blockSelectOffset;
        } else if (mainThreadData.selectedPass) {
                blockSelected = 1;
                blockSelect = mainThreadData.coordPass;
                blockSelectOffset = mainThreadData.blockSelectOffset;
        } else {
                blockSelected = 0;
        }

        // Process block breaking/placing with time-based cooldown
        if (gamePopup == POPUP_HUD && blockSelected) {
                InvSlot *activeSlot = &player->inventory.hotbar[player->inventory.hotbarSelect];

                if (inputs->mouse.left > 0) {
                        uint32_t now = SDL_GetTicks();
                        if (now - lastBreakTime >= 100) {
                                Block blockid = World_getBlock(&world, blockSelect.x, blockSelect.y, blockSelect.z);
                                if (blockid != BLOCK_PLAYER_BODY && blockid != BLOCK_PLAYER_HEAD) {
                                        InvSlot pickedUp = { .blockid = blockid, .amount = 1, .durability = 1 };
                                        Inventory_transferIn(&player->inventory, &pickedUp);
                                        World_setBlock(&world, blockSelect.x, blockSelect.y, blockSelect.z, 0, 1);
                                }
                                lastBreakTime = now;
                        }
                } else {
                        lastBreakTime = 0;
                }

                blockSelectOffset.x += blockSelect.x;
                blockSelectOffset.y += blockSelect.y;
                blockSelectOffset.z += blockSelect.z;

                if (inputs->mouse.right > 0) {
                        if (
                                (fabs(player->pos.x - 0.5 - blockSelectOffset.x) >= 0.8 ||
                                 fabs(player->pos.y - blockSelectOffset.y) >= 1.45 ||
                                 fabs(player->pos.z - 0.5 - blockSelectOffset.z) >= 0.8) &&
                                activeSlot->amount > 0
                        ) {
                                int blockSet = World_setBlock(&world, blockSelectOffset.x, blockSelectOffset.y, blockSelectOffset.z, activeSlot->blockid, 1);
                                if (blockSet) {
                                        activeSlot->amount--;
                                        if (activeSlot->amount == 0) activeSlot->blockid = 0;
                                }
                        }
                }
        }

        inputs->mouse.left = 0;
        inputs->mouse.right = 0;

        if (inputs->keyboard.f1) { inputs->keyboard.f1 = 0; guiOn ^= 1; }
        if (inputs->keyboard.f3) { inputs->keyboard.f3 = 0; debugOn = !debugOn; }
        if (inputs->keyboard.e) { inputs->keyboard.e = 0; gamePopup = POPUP_INVENTORY; }

        if (headInWater) {
                SDL_SetRenderDrawColor(renderer, 16, 32, 255, 128);
                SDL_RenderFillRect(renderer, &backgroundRect);
        }

        inputs->mouse.x /= BUFFER_SCALE;
        inputs->mouse.y /= BUFFER_SCALE;

        gameLoop_drawPopup(renderer, inputs);
}

void gameLoop_drawPopup (SDL_Renderer *renderer, Inputs *inputs) {
        if (gamePopup != 0) {
                SDL_SetRelativeMouseMode(0);
        }

        switch (gamePopup) {
        case POPUP_HUD:
                if (options.trapMouse) SDL_SetRelativeMouseMode(1);
                if (guiOn) popup_hud(renderer, inputs, &world, &debugOn, &fps_now, player);
                break;
        case POPUP_PAUSE:
                tblack(renderer);
                SDL_RenderFillRect(renderer, &backgroundRect);
                popup_pause(renderer, inputs, &gamePopup, &gameState, &world);
                break;
        case POPUP_OPTIONS:
                tblack(renderer);
                SDL_RenderFillRect(renderer, &backgroundRect);
                popup_options(renderer, inputs, &gamePopup);
                break;
        case POPUP_INVENTORY:
                popup_inventory(renderer, inputs, player, &gamePopup);
                break;
        #ifndef small
        case POPUP_ADVANCED_DEBUG:
                tblack(renderer);
                SDL_RenderFillRect(renderer, &backgroundRect);
                popup_debugTools(renderer, inputs, &gamePopup);
                break;
        case POPUP_CHUNK_PEEK:
                tblack(renderer);
                SDL_RenderFillRect(renderer, &backgroundRect);
                popup_chunkPeek(renderer, inputs, &world, &gamePopup, player);
                break;
        case POPUP_ROLL_CALL:
                tblack(renderer);
                SDL_RenderFillRect(renderer, &backgroundRect);
                popup_rollCall(renderer, inputs, &world, &gamePopup);
                break;
        case POPUP_OVERVIEW:
                tblack(renderer);
                SDL_RenderFillRect(renderer, &backgroundRect);
                popup_overview(renderer, inputs, &world, &gamePopup);
                break;
        #endif
        case POPUP_CHAT:
                popup_chat(renderer, inputs, world.time);
                break;
        }
}

static void gameLoop_processMovement (Inputs *inputs, int inWater) {
        static int flipFlop = 0;
        flipFlop = !flipFlop;
        int doPhysics = !inWater || flipFlop;

        if (gamePopup == 0) {
                if (options.trapMouse) {
                        player->hRot += (double)inputs->mouse.x / 64.0;
                        player->vRot -= (double)inputs->mouse.y / 64.0;
                }

                if (player->vRot < -1.57) player->vRot = -1.57;
                if (player->vRot >  1.57) player->vRot =  1.57;

                double speed = 0.02;

                if (doPhysics) {
                        player->FBVelocity = (inputs->keyboard.w - inputs->keyboard.s) * speed;
                        player->LRVelocity = (inputs->keyboard.d - inputs->keyboard.a) * speed;
                }
        }
        
        static Coords playerMovement = { 0.0, 0.0, 0.0 };

        if (doPhysics) {
                playerMovement.x *= 0.5;
                playerMovement.y *= 0.99;
                playerMovement.z *= 0.5;

                playerMovement.x += player->vectorH.x * player->FBVelocity + player->vectorH.y * player->LRVelocity;
                playerMovement.z += player->vectorH.y * player->FBVelocity - player->vectorH.x * player->LRVelocity;
                playerMovement.y += 0.003;
        }

        for (int axis = 0; axis < 3; axis++) {
                if (!doPhysics) break;

                Coords playerPosTry = {
                        player->pos.x + playerMovement.x * (double)((axis + 2) % 3 / 2),
                        player->pos.y + playerMovement.y * (double)((axis + 1) % 3 / 2),
                        player->pos.z + playerMovement.z * (double)((axis + 3) % 3 / 2),
                };
                
                for (int step = 0; step < 12; step++) {
                        int blockX = (int)floor(playerPosTry.x + ((step >> 0) & 1) * 0.6 - 0.3);
                        int blockY = (int)floor(playerPosTry.y + ((step >> 2) - 1) * 0.8 + 0.65);
                        int blockZ = (int)floor(playerPosTry.z + ((step >> 1) & 1) * 0.6 - 0.3);

                        Block block = World_getBlock(&world, blockX, blockY, blockZ);

                        int shouldCollide = 1;
                        shouldCollide &= block != BLOCK_AIR;
                        shouldCollide &= block != BLOCK_WATER;
                        shouldCollide &= block != BLOCK_TALL_GRASS;
                        
                        if (shouldCollide) {
                                if (axis != 1) goto stopCheck;
                                if (inputs->keyboard.space > 0 && playerMovement.y > 0.0 && !gamePopup) {
                                        inputs->keyboard.space = 0;
                                        playerMovement.y = -0.1;
                                        goto stopCheck;
                                }
                                playerMovement.y = 0.0;
                                goto stopCheck;
                        }
                }

                player->pos.x = playerPosTry.x;
                player->pos.y = playerPosTry.y;
                player->pos.z = playerPosTry.z;

                stopCheck:;
        }

        if (inWater && doPhysics) {
                if (inputs->keyboard.space > 0 && playerMovement.y > -0.05 && !gamePopup) {
                        inputs->keyboard.space = 0;
                        playerMovement.y = -0.1;
                }
        }
}

int gameLoop_screenshot (SDL_Renderer *renderer, const char *path) {
        SDL_Surface *grab = SDL_CreateRGBSurfaceWithFormat(
                0, BUFFER_W * BUFFER_SCALE, BUFFER_H * BUFFER_SCALE,
                32, SDL_PIXELFORMAT_ARGB8888
        );

        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, grab->pixels, grab->pitch);

        if (path == NULL) {
                chatAdd("Couldn't save screenshot");
                return 1;
        }
        
        int saved = SDL_SaveBMP(grab, path);
        SDL_FreeSurface(grab);

        if (saved == 0) {
                chatAdd("Saved screenshot");
                return 0;
        } else {
                chatAdd("Couldn't save screenshot");
                return 1;
        }
}

void gameLoop_error (char *message) {
        errorMessage = message;
}