#include <time.h>
#include <vpad/input.h>
#include "menus.h"
#include "data.h"
#include "blocks.h"
#include "options.h"
#include "gameloop.h"

static int menu_optionsMain (SDL_Renderer *, Inputs *);

/* VPAD button state tracking */
static uint32_t vpadHold = 0;
static uint32_t vpadTrigger = 0;
static uint32_t vpadRelease = 0;

/* vpadReadInput
 * Reads VPAD data and updates the global button state.
 */
static void vpadReadInput (void) {
        VPADStatus vpadStatus;
        VPADReadError vpadError;
        int vpadRead = VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);

        if (vpadRead > 0 && vpadError == VPAD_READ_SUCCESS) {
                vpadHold    = vpadStatus.hold;
                vpadTrigger = vpadStatus.trigger;
                vpadRelease = vpadStatus.release;
        }
}

/* vpadButtonPressed
 * Returns 1 if the specified button was just pressed this frame.
 */
static int vpadButtonPressed (uint32_t button) {
        return (vpadTrigger & button) ? 1 : 0;
}

/* vpadButtonHeld
 * Returns 1 if the specified button is currently held.
 */
static int vpadButtonHeld (uint32_t button) {
        return (vpadHold & button) ? 1 : 0;
}

/* drawMenuCursor
 * Draws a selection arrow next to the currently highlighted menu item.
 */
static void drawMenuCursor (SDL_Renderer *renderer, int x, int y) {
        white(renderer);
        drawChar(renderer, '>', x, y);
}

/* drawMenuTitle
 * Draws a centered title at the top of the screen.
 */
static void drawMenuTitle (SDL_Renderer *renderer, const char *title) {
        white(renderer);
        drawBig(renderer, title, BUFFER_HALF_W, 8);
}

/* drawMenuButton
 * Draws a menu button and returns 1 if it's the currently selected item.
 */
static int drawMenuButton (SDL_Renderer *renderer, const char *text, int x, int y, int w, int selected) {
        if (selected) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
                SDL_Rect bg = { x, y, w, 18 };
                SDL_RenderFillRect(renderer, &bg);
                drawMenuCursor(renderer, x - 12, y + 4);
        }
        white(renderer);
        drawStr(renderer, text, x + 4, y + 4);
        return selected;
}

/* drawMenuSlider
 * Draws a menu option with < and > arrows for cycling through values.
 * Returns 1 if left arrow was pressed, 2 if right arrow was pressed.
 */
static int drawMenuSlider (SDL_Renderer *renderer, const char *text, int x, int y, int w, int selected) {
        int result = 0;
        
        if (selected) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
                SDL_Rect bg = { x, y, w, 18 };
                SDL_RenderFillRect(renderer, &bg);
                drawMenuCursor(renderer, x - 12, y + 4);
                
                if (vpadButtonPressed(VPAD_BUTTON_LEFT)) {
                        result = 1;
                }
                if (vpadButtonPressed(VPAD_BUTTON_RIGHT)) {
                        result = 2;
                }
        }
        white(renderer);
        drawStr(renderer, text, x + 4, y + 4);
        return result;
}

/* === GAME STATES === */

/* state_title
 * Presents a title screen with basic options.
 */
int state_title (SDL_Renderer *renderer, Inputs *inputs, int *gameState) {
        vpadReadInput();
        (void)(inputs);

        static int selection = 0;
        int maxSelection = 1;

        if (vpadButtonPressed(VPAD_BUTTON_UP)) {
                selection--;
                if (selection < 0) selection = maxSelection;
        }
        if (vpadButtonPressed(VPAD_BUTTON_DOWN)) {
                selection++;
                if (selection > maxSelection) selection = 0;
        }

        dirtBg(renderer);
        drawMenuTitle(renderer, "M4KCU");

        #ifdef small
        shadowStr(renderer, "Wii U Port (V0.7)", 10, BUFFER_H - 10);
        #else
        shadowStr(renderer, "Wii U Port (V0.7)", 10, BUFFER_H - 10);
        #endif

        drawMenuButton(renderer, "Play", BUFFER_HALF_W - 64, BUFFER_HALF_H - 10, 128, selection == 0);
        //drawMenuButton(renderer, "Options", BUFFER_HALF_W - 64, 64, 128, selection == 1);

        if (vpadButtonPressed(VPAD_BUTTON_A)) {
                switch (selection) {
                case 0:
                        if (data_refreshWorldList()) {
                                gameLoop_error("Cannot refresh world list");
                        } else {
                                *gameState = STATE_SELECT_WORLD;
                        }
                        break;
                //case 1:
                //        *gameState = STATE_OPTIONS;
                //        break;
                }
        }

        return 0;
}

/* state_selectWorld
 * Shows a list of saved worlds to play or delete.
 */
void state_selectWorld (
        SDL_Renderer *renderer,
        Inputs *inputs,
        int *gameState,
        World *world
) {
        vpadReadInput();
        (void)(inputs);

        static int scroll = 0;
        static int selection = 0;
        int needRefresh = 0;

        int totalItems = data_worldListLength + 2; // worlds + Cancel + New

        if (vpadButtonPressed(VPAD_BUTTON_UP)) {
                selection--;
                if (selection < 0) selection = totalItems - 1;
                if (selection < scroll) scroll = selection;
        }
        if (vpadButtonPressed(VPAD_BUTTON_LEFT)) {
                selection--;
                if (selection < 0) selection = totalItems - 1;
                if (selection < scroll) scroll = selection;
        }
        if (vpadButtonPressed(VPAD_BUTTON_DOWN)) {
                selection++;
                if (selection >= totalItems) selection = 0;
                if (selection >= scroll + ((BUFFER_H - 72) / 21)) {
                        scroll = selection - ((BUFFER_H - 72) / 21) + 1;
                }
        }
        if (vpadButtonPressed(VPAD_BUTTON_RIGHT)) {
                selection++;
                if (selection >= totalItems) selection = 0;
                if (selection >= scroll + ((BUFFER_H - 72) / 21)) {
                        scroll = selection - ((BUFFER_H - 72) / 21) + 1;
                }
        }
        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                *gameState = STATE_TITLE;
                selection = 0;
                scroll = 0;
                return;
        }

        if (scroll < 0) scroll = 0;

        SDL_Rect listBackground;
        listBackground.x = 0;
        listBackground.y = 0;
        listBackground.w = BUFFER_W;
        listBackground.h = BUFFER_H - 28;

        dirtBg(renderer);
        tblack(renderer);
        SDL_RenderFillRect(renderer, &listBackground);
        SDL_RenderDrawLine(renderer,
                0,        BUFFER_H - 29,
                BUFFER_W, BUFFER_H - 29);

        drawMenuTitle(renderer, "Select World");

        int index = 0;
        int y = 30;
        int yLimit = BUFFER_H - 44;
        data_WorldListItem *item = data_worldList;
        
        // Draw world list items
        while (item != NULL) {
                if (y > yLimit) break;
                if (index < scroll) { goto nextItem; }

                int isSelected = (selection == index);
                if (isSelected) {
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
                        SDL_Rect bg = { BUFFER_HALF_W - 64, y, 128, 18 };
                        SDL_RenderFillRect(renderer, &bg);
                        drawMenuCursor(renderer, BUFFER_HALF_W - 76, y + 4);
                }
                white(renderer);
                drawStr(renderer, item->name, BUFFER_HALF_W - 56, y + 4);
                y += 21;

                if (vpadButtonPressed(VPAD_BUTTON_X) && isSelected) {
                        char deletePath[PATH_MAX];
                        if (data_getWorldPath(deletePath, item->name)) {
                                gameLoop_error("Could not delete world");
                                return;
                        }
                        data_removeDirectory(deletePath);
                        needRefresh = 1;
                }

                nextItem:
                index++;
                item = item->next;
        }

        // Show "No worlds" if list is empty
        if (data_worldListLength == 0 && index == 0) {
                shadowCenterStr(renderer, "No worlds", BUFFER_HALF_W, BUFFER_HALF_H - 15);
                y = BUFFER_HALF_H;
        }

        // Add some spacing before the buttons
        if (y < BUFFER_H - 50) {
                y = BUFFER_H - 50;
        }

        // Cancel button
        {
                int isSelected = (selection == index);
                drawMenuButton(renderer, "Cancel", BUFFER_HALF_W - 64, y, 61, isSelected);
        }
        // New button
        {
                int isSelected = (selection == index + 1);
                drawMenuButton(renderer, "New", BUFFER_HALF_W + 3, y, 61, isSelected);
        }

        // Handle A button presses
        if (vpadButtonPressed(VPAD_BUTTON_A)) {
                if (selection < data_worldListLength) {
                        // A world was selected
                        item = data_worldList;
                        for (int i = 0; i < selection && item; i++) {
                                item = item->next;
                        }
                        if (item) {
                                if (World_load(world, item->name)) {
                                        gameLoop_error("Could not load world");
                                } else {
                                        *gameState = STATE_LOADING;
                                }
                        }
                } else if (selection == data_worldListLength) {
                        // Cancel button
                        *gameState = STATE_TITLE;
                        selection = 0;
                        scroll = 0;
                } else if (selection == data_worldListLength + 1) {
                        // New button
                        *gameState = STATE_NEW_WORLD;
                        selection = 0;
                        scroll = 0;
                }
        }

        if (needRefresh) {
                data_refreshWorldList();
                totalItems = data_worldListLength + 2;
                if (selection >= totalItems) {
                        selection = totalItems - 1;
                }
        }
}

const char *terrainNames[16] = {
        "Classic Terrain",
        "Natural Terrain",
        "Flat Stone",
        "Flat Grass",
        "Water World"
};

const char *dayNightModes[16] = {
        "Day and Night",
        "Always Day",
        "Always Night",
};

/* state_newWorld
 * Shows a menu for creating a new world.
 */
void state_newWorld (
        SDL_Renderer *renderer,
        Inputs *inputs,
        int *gameState,
        World *world
) {
        vpadReadInput();
        (void)(inputs);

        static int selection = 0;
        static int typeSelect = 1;
        static int dayNightSelect = 0;
        static int badName = 0;

        static char seedBuffer[16] = "";
        static char nameBuffer[16] = "World";
        static int nameCursor = 0;
        static int seedCursor = 0;

        int maxSelection = 4; // Name, Seed, Terrain, Day/Night, Cancel, Generate

        if (vpadButtonPressed(VPAD_BUTTON_UP)) {
                selection--;
                if (selection < 0) selection = maxSelection;
        }
        if (vpadButtonPressed(VPAD_BUTTON_DOWN)) {
                selection++;
                if (selection > maxSelection) selection = 0;
        }
        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                *gameState = STATE_SELECT_WORLD;
                selection = 0;
                return;
        }

        dirtBg(renderer);

        // Name field
        {
                char displayName[32];
                if (nameBuffer[0] == 0) {
                        snprintf(displayName, 32, "Name:      ");
                } else {
                        snprintf(displayName, 32, "Name: %s", nameBuffer);
                }
                if (selection == 0 && vpadButtonPressed(VPAD_BUTTON_A)) {
                        if (nameBuffer[0] == 0) {
                                nameBuffer[0] = 'My-World';
                                nameCursor = 1;
                        }
                }
                drawMenuButton(renderer, displayName, BUFFER_HALF_W - 64, 20, 128, selection == 0);
        }

        // Seed field
        /*{
                char displaySeed[32];
                if (seedBuffer[0] == 0) {
                        snprintf(displaySeed, 32, "Seed: (random)");
                } else {
                        snprintf(displaySeed, 32, "Seed: %s", seedBuffer);
                }
                if (selection == 1 && vpadButtonPressed(VPAD_BUTTON_A)) {
                        if (seedBuffer[0] == 0) {
                                snprintf(seedBuffer, 16, "12345");
                                seedCursor = 5;
                        } else {
                                seedBuffer[0] = 0;
                                seedCursor = 0;
                        }
                }
                drawMenuButton(renderer, displaySeed, BUFFER_HALF_W - 64, 52, 128, selection == 1);
        }*/

        // Terrain type
        if (selection == 1) {
                if (vpadButtonPressed(VPAD_BUTTON_LEFT)) {
                        typeSelect--;
                        if (typeSelect < 0) typeSelect = 4;
                }
                if (vpadButtonPressed(VPAD_BUTTON_RIGHT)) {
                        typeSelect++;
                        if (typeSelect > 4) typeSelect = 0;
                }
                if (vpadButtonPressed(VPAD_BUTTON_A)) {
                        typeSelect++;
                        if (typeSelect > 4) typeSelect = 0;
                }
        }
        drawMenuSlider(renderer, terrainNames[typeSelect], BUFFER_HALF_W - 64, 50, 128, selection == 1);

        // Day/Night mode
        if (selection == 2) {
                if (vpadButtonPressed(VPAD_BUTTON_LEFT)) {
                        dayNightSelect--;
                        if (dayNightSelect < 0) dayNightSelect = 2;
                }
                if (vpadButtonPressed(VPAD_BUTTON_RIGHT)) {
                        dayNightSelect++;
                        if (dayNightSelect > 2) dayNightSelect = 0;
                }
                if (vpadButtonPressed(VPAD_BUTTON_A)) {
                        dayNightSelect++;
                        if (dayNightSelect > 2) dayNightSelect = 0;
                }
        }
        drawMenuSlider(renderer, dayNightModes[dayNightSelect], BUFFER_HALF_W - 64, 70, 128, selection == 2);

        // Cancel
        drawMenuButton(renderer, "Cancel", BUFFER_HALF_W - 64, 90, 61, selection == 3);
        if (selection == 3 && vpadButtonPressed(VPAD_BUTTON_A)) {
                *gameState = STATE_SELECT_WORLD;
                selection = 0;
        }
        if (selection == 3 && vpadButtonPressed(VPAD_BUTTON_RIGHT)) {
                selection++;
        }

        // Generate
        drawMenuButton(renderer, "Create", BUFFER_HALF_W + 3, 90, 61, selection == 4);
        if (selection == 4 && vpadButtonPressed(VPAD_BUTTON_LEFT)) {
                selection--;
        }
        if (selection == 4 && vpadButtonPressed(VPAD_BUTTON_A)) {
                // Default if empty
                if (nameBuffer[0] == 0) {
                        nameBuffer[0] = 'My World';
                        nameBuffer[1] = 0;
                }
                // Reject names with slashes
                int invalidName = 0;
                for (int i = 0; nameBuffer[i]; i++) {
                        if (nameBuffer[i] == '/') {
                                invalidName = 1;
                                break;
                        }
                }
                if (invalidName) {
                        badName = 1;
                        nameBuffer[0] = 0;
                        return;
                }

                if (data_getWorldPath(world->path, nameBuffer)) {
                        badName = 1;
                        nameBuffer[0] = 0;
                        return;
                }

                if (data_directoryExists(world->path)) {
                        badName = 1;
                        nameBuffer[0] = 0;
                        return;
                }

                world->time         = 2048;
                world->type         = typeSelect;
                world->dayNightMode = dayNightSelect;

                // Get numeric seed
                world->seed = 0;
                for (int i = 0; seedBuffer[i]; i++) {
                        world->seed *= 10;
                        world->seed += seedBuffer[i] - '0';
                }

                // "Randomize" seed if it was not set
                if (world->seed == 0) {
                        world->seed = time(0) % 999999999999999;
                }

                // Secret world for testing
                if (world->seed == 5800) {
                        world->type = -1;
                }

                seedBuffer[0] = 0;
                seedCursor = 0;
                nameBuffer[0] = 0;
                nameCursor = 0;
                badName = 0;
                selection = 0;

                *gameState = STATE_LOADING;
        }

        if (badName) {
                SDL_SetRenderDrawColor(renderer, 255, 128, 128, 255);
                drawStr(renderer, "Invalid name!", BUFFER_HALF_W - 40, 140);
        }
}

/* state_loading
 * Shows a loading screen and progressively loads in chunks. Returns 1 when finished.
 */
int state_loading (
        SDL_Renderer *renderer,
        World *world,
        unsigned int seed,
        Coords center
) {
        IntCoords chunkLoadCoords;
        static int chunkLoadNum = 0;

        if (chunkLoadNum < CHUNKARR_SIZE) {
                chunkLoadCoords.x =
                        ((chunkLoadNum % CHUNKARR_DIAM) -
                        CHUNKARR_RAD) * 64;
                chunkLoadCoords.y =
                        (((chunkLoadNum / CHUNKARR_DIAM) % CHUNKARR_DIAM) -
                        CHUNKARR_RAD) * 64;
                chunkLoadCoords.z =
                        ((chunkLoadNum / (CHUNKARR_DIAM * CHUNKARR_DIAM)) -
                        CHUNKARR_RAD) * 64;
                genChunk (
                        world, seed,
                        chunkLoadCoords.x,
                        chunkLoadCoords.y,
                        chunkLoadCoords.z, world->type, 1,
                        center
                );
                loadScreen (
                        renderer,
                        "Generating world...",
                        chunkLoadNum, CHUNKARR_SIZE
                );
                chunkLoadNum++;
                return 0;
        } else {
                chunkLoadNum = 0;
                return 1;
        }
}

/* state_options
 * Shows an options screen.
 */
void state_options (SDL_Renderer *renderer, Inputs *inputs, int *gameState) {
        vpadReadInput();

        dirtBg(renderer);
        drawMenuTitle(renderer, "Options");

        if (menu_optionsMain(renderer, inputs)) {
                *gameState = STATE_TITLE;
        }
}

/* state_egg
 * This lacks description.
 */
void state_egg (SDL_Renderer *renderer, Inputs *inputs, int *gameState) {
        vpadReadInput();
        (void)(inputs);

        dirtBg(renderer);
        white(renderer);
        centerStr(renderer, "Go away, this is my house.", BUFFER_HALF_W, BUFFER_HALF_H - 16);

        if (vpadButtonPressed(VPAD_BUTTON_A)) {
                *gameState = STATE_TITLE;
        }
}

/* state_err
 * Shows an error message on screen.
 */
int state_err (SDL_Renderer *renderer, Inputs *inputs, char *message) {
        vpadReadInput();
        (void)(inputs);

        dirtBg(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 128, 128, 255);
        centerStr(renderer, "Error:", BUFFER_HALF_W, BUFFER_HALF_H - 20);
        white(renderer);
        centerStr(renderer, message, BUFFER_HALF_W, BUFFER_HALF_H - 4);

        if (vpadButtonPressed(VPAD_BUTTON_A)) {
                return 1;
        }
        return 0;
}

/* === INGAME POPUPS === */

/* popup_hud
 * Draws the heads up display.
 */
void popup_hud (
        SDL_Renderer *renderer, Inputs *inputs, World *world,
        int *debugOn, uint32_t *fps_now,
        Player *player
) {
        int i;

        static SDL_Rect hotbarRect;
        hotbarRect.x = BUFFER_HALF_W - 77;
        hotbarRect.y = BUFFER_H - 18;
        hotbarRect.w = 154;
        hotbarRect.h = 18;

        static SDL_Rect hotbarSelectRect;
        hotbarSelectRect.x = 0;
        hotbarSelectRect.y = hotbarRect.y;
        hotbarSelectRect.w = 18;
        hotbarSelectRect.h = 18;

        static SDL_Rect offhandRect;
        offhandRect.x = 0;
        offhandRect.y = BUFFER_H - 18;
        offhandRect.w = 18;
        offhandRect.h = 18;

        // Debug screen
        if (*debugOn) {
                static char debugText [][32] = {
                        "M4KC 0.7 (Wii U)",
                        "Seed: ",
                        "X: ",
                        "Y: ",
                        "Z: ",
                        "FPS: ",
                        "ChunkX: ",
                        "ChunkY: ",
                        "ChunkZ: "
                };

                strnum(debugText[1], 6, world->seed);
                strnum(debugText[2], 3, (int)player->pos.x);
                strnum(debugText[3], 3, (int)player->pos.y);
                strnum(debugText[4], 3, (int)player->pos.z);
                strnum(debugText[5], 5, *fps_now);
                strnum(debugText[6], 8, ((int)player->pos.x) >> 6);
                strnum(debugText[7], 8, ((int)player->pos.y) >> 6);
                strnum(debugText[8], 8, ((int)player->pos.z) >> 6);

                for (i = 0; i < 9; i++) {
                        drawBGStr(renderer, debugText[i], 0, i * 9);
                }

                #ifndef small
                #define CHUNKMONW   10
                #define CHUNKMONCOL 9

                SDL_Rect chunkMonitorRect = {
                        .x = 0,
                        .y = 1 - CHUNKMONW,
                        .w = CHUNKMONW,
                        .h = CHUNKMONW
                };
                for (i = 0; i < CHUNKARR_SIZE; i++) {
                        if (i % CHUNKMONCOL == 0) {
                                chunkMonitorRect.x = BUFFER_W - (
                                        (CHUNKMONW * (CHUNKMONCOL - 1)) + 2);
                                chunkMonitorRect.y += CHUNKMONW - 1;
                        } else {
                                chunkMonitorRect.x += CHUNKMONW - 1;
                        }

                        int stamp = world->chunk[i].loaded;
                        SDL_SetRenderDrawColor (
                                renderer,
                                (stamp & 0x03) * 64,
                                (stamp & 0x0C) * 16,
                                (stamp & 0x30) * 4,
                                0xFF
                        );
                        SDL_RenderFillRect(renderer, &chunkMonitorRect);
                        white(renderer);
                        SDL_RenderDrawRect(renderer, &chunkMonitorRect);
                }

                #undef CHUNKMONW
                #undef CHUNKMONCOL
                #endif
        }

        // Hotbar
        tblack(renderer);
        SDL_RenderFillRect(renderer, &hotbarRect);

        hotbarSelectRect.x =
        BUFFER_HALF_W - 77 + player->inventory.hotbarSelect * 17;
        white(renderer);
        SDL_RenderDrawRect(renderer, &hotbarSelectRect);

        for (i = 0; i < 9; i++) {
                drawSlot (
                        renderer,
                        &player->inventory.hotbar[i],
                        BUFFER_HALF_W - 76 + i * 17,
                        BUFFER_H - 17,
                        inputs->mouse.x,
                        inputs->mouse.y
                );
        }

        // Offhand
        if (player->inventory.offhand.blockid != 0) {
                tblack(renderer);
                SDL_RenderDrawRect(renderer, &offhandRect);
                drawSlot (
                        renderer,
                        &player->inventory.offhand,
                        1,
                        BUFFER_H - 17,
                        inputs->mouse.x,
                        inputs->mouse.y
                );
        }

        // Chat
        int chatDrawIndex = chatHistoryIndex;
        for (i = 0; i < 11; i++) {
                chatDrawIndex = nmod(chatDrawIndex - 1, 11);
                if (chatHistoryFade[chatDrawIndex] > 0) {
                        chatHistoryFade[chatDrawIndex]--;
                        drawBGStr(
                                renderer, chatHistory[chatDrawIndex],
                                0, BUFFER_H - 32 - i * 9
                        );
                }
        }
}

/* manageInvSlot
 * Draws and performs the input logic of a single inventory slot.
 */
void manageInvSlot (
        SDL_Renderer *renderer,
        Inputs  *inputs,
        int     x,
        int     y,
        InvSlot *current,
        InvSlot *selected,
        int     *dragging
) {
        if (drawSlot (
                renderer,
                current,
                x, y,
                inputs->mouse.x,
                inputs->mouse.y
        ) && inputs->mouse.left) {
                inputs->mouse.left = 0;
                if (*dragging) {
                        if (current->blockid == 0) {
                                *current  = *selected;
                                *selected = (const InvSlot) { 0 };
                                *dragging = 0;
                        } else if (current->blockid == selected->blockid) {
                                InvSlot_transfer(current, selected);
                        } else {
                                InvSlot_swap(current, selected);
                        }
                } else if (current->blockid != 0) {
                        *selected = *current;
                        *current  = (const InvSlot) { 0 };
                        *dragging = 1;
                }
        }
}

/* popup_inventory
 * Allows the user to manage their inventory with D-pad navigation.
 */
void popup_inventory (
        SDL_Renderer *renderer,
        Inputs *inputs,
        Player *player,
        int *gamePopup
) {
        vpadReadInput();

        static int selectionX = 4;
        static int selectionY = 2;
        static InvSlot selected = { 0 };
        static int dragging = 0;

        SDL_Rect inventoryRect;
        inventoryRect.x = BUFFER_HALF_W - 77;
        inventoryRect.y = (BUFFER_H - 18) / 2 - 26;
        inventoryRect.w = 154;
        inventoryRect.h = 52;

        SDL_Rect hotbarRect;
        hotbarRect.x = BUFFER_HALF_W - 77;
        hotbarRect.y = BUFFER_H - 18;
        hotbarRect.w = 154;
        hotbarRect.h = 18;

        SDL_Rect offhandRect;
        offhandRect.x = 0;
        offhandRect.y = BUFFER_H - 18;
        offhandRect.w = 18;
        offhandRect.h = 18;

        // D-pad navigation
        if (vpadButtonPressed(VPAD_BUTTON_UP)) {
                selectionY--;
                if (selectionY < 0) selectionY = 3;
        }
        if (vpadButtonPressed(VPAD_BUTTON_DOWN)) {
                selectionY++;
                if (selectionY > 3) selectionY = 0;
        }
        if (vpadButtonPressed(VPAD_BUTTON_LEFT)) {
                selectionX--;
                if (selectionX < 0) selectionX = 8;
        }
        if (vpadButtonPressed(VPAD_BUTTON_RIGHT)) {
                selectionX++;
                if (selectionX > 8) selectionX = 0;
        }

        // Inventory background
        tblack(renderer);
        SDL_RenderFillRect(renderer, &inventoryRect);
        SDL_RenderFillRect(renderer, &hotbarRect);
        SDL_RenderFillRect(renderer, &offhandRect);

        // Hotbar items
        for (int i = 0; i < HOTBAR_SIZE; i++) {
                int slotX = BUFFER_HALF_W - 76 + i * 17;
                int slotY = BUFFER_H - 17;

                if (selectionY == 3 && selectionX == i) {
                        white(renderer);
                        SDL_Rect cursor = { slotX - 1, slotY - 1, 18, 18 };
                        SDL_RenderDrawRect(renderer, &cursor);
                }

                drawSlot (
                        renderer,
                        &player->inventory.hotbar[i],
                        slotX, slotY,
                        0, 0
                );

                // A button to pick up/place
                if (selectionY == 3 && selectionX == i && vpadButtonPressed(VPAD_BUTTON_A)) {
                        if (dragging) {
                                if (player->inventory.hotbar[i].blockid == 0) {
                                        player->inventory.hotbar[i] = selected;
                                        selected = (const InvSlot) { 0 };
                                        dragging = 0;
                                } else if (player->inventory.hotbar[i].blockid == selected.blockid) {
                                        InvSlot_transfer(&player->inventory.hotbar[i], &selected);
                                } else {
                                        InvSlot_swap(&player->inventory.hotbar[i], &selected);
                                }
                        } else if (player->inventory.hotbar[i].blockid != 0) {
                                selected = player->inventory.hotbar[i];
                                player->inventory.hotbar[i] = (const InvSlot) { 0 };
                                dragging = 1;
                        }
                }
        }

        // Inventory items
        for (int i = 0; i < INVENTORY_SIZE; i++) {
                int slotX = BUFFER_HALF_W - 76 + (i % HOTBAR_SIZE) * 17;
                int slotY = inventoryRect.y + 1 + (i / HOTBAR_SIZE) * 17;
                int gridX = i % HOTBAR_SIZE;
                int gridY = i / HOTBAR_SIZE;

                if (selectionY == gridY && selectionX == gridX) {
                        white(renderer);
                        SDL_Rect cursor = { slotX - 1, slotY - 1, 18, 18 };
                        SDL_RenderDrawRect(renderer, &cursor);
                }

                drawSlot (
                        renderer,
                        &player->inventory.slots[i],
                        slotX, slotY,
                        0, 0
                );

                if (selectionY == gridY && selectionX == gridX && vpadButtonPressed(VPAD_BUTTON_A)) {
                        if (dragging) {
                                if (player->inventory.slots[i].blockid == 0) {
                                        player->inventory.slots[i] = selected;
                                        selected = (const InvSlot) { 0 };
                                        dragging = 0;
                                } else if (player->inventory.slots[i].blockid == selected.blockid) {
                                        InvSlot_transfer(&player->inventory.slots[i], &selected);
                                } else {
                                        InvSlot_swap(&player->inventory.slots[i], &selected);
                                }
                        } else if (player->inventory.slots[i].blockid != 0) {
                                selected = player->inventory.slots[i];
                                player->inventory.slots[i] = (const InvSlot) { 0 };
                                dragging = 1;
                        }
                }
        }

        // Offhand
        drawSlot (
                renderer,
                &player->inventory.offhand,
                1, BUFFER_H - 17,
                0, 0
        );

        if (dragging) {
                drawSlot (
                        renderer,
                        &selected,
                        BUFFER_HALF_W - 8,
                        (BUFFER_H - 18) / 2 - 34,
                        0, 0
                );
        }

        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                if (dragging) {
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                                if (player->inventory.slots[i].blockid == 0) {
                                        player->inventory.slots[i] = selected;
                                        selected = (const InvSlot) { 0 };
                                        dragging = 0;
                                        break;
                                }
                        }
                }
                *gamePopup = POPUP_HUD;
        }
}

/* popup_chat
 * What is the point of chat on this game...
 * Is there even multiplayer???
 */
void popup_chat (SDL_Renderer *renderer, Inputs *inputs, uint64_t gameTime) {
        (void)(gameTime);
        (void)(inputs);
        
        int chatDrawIndex = chatHistoryIndex;
        for (int i = 0; i < 11; i++) {
                chatDrawIndex = nmod(chatDrawIndex - 1, 11);
                drawBGStr(
                        renderer, chatHistory[chatDrawIndex],
                        0, BUFFER_H - 32 - i * 9
                );
        }
}

/* popup_pause
 * Displays a pause menu with D-pad navigation.
 */
void popup_pause (
        SDL_Renderer *renderer, Inputs *inputs,
        int *gamePopup, int *gameState, World *world
) {
        vpadReadInput();
        (void)(inputs);

        static int selection = 0;

        if (vpadButtonPressed(VPAD_BUTTON_UP)) {
                selection--;
                if (selection < 0) selection = 2;
        }
        if (vpadButtonPressed(VPAD_BUTTON_DOWN)) {
                selection++;
                if (selection > 2) selection = 0;
        }
        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                *gamePopup = POPUP_HUD;
                selection = 0;
                return;
        }

        tblack(renderer);
        SDL_Rect bg = { 0, 0, BUFFER_W, BUFFER_H };
        SDL_RenderFillRect(renderer, &bg);

        drawMenuTitle(renderer, "Paused");

        drawMenuButton(renderer, "Back to Game", BUFFER_HALF_W - 64, 30, 128, selection == 0);
        //drawMenuButton(renderer, "Options...", BUFFER_HALF_W - 64, 52, 128, selection == 1);
        drawMenuButton(renderer, "Save and Quit", BUFFER_HALF_W - 64, 74, 128, selection == 1); //== 2);

        if (vpadButtonPressed(VPAD_BUTTON_A)) {
                switch (selection) {
                case 0:
                        *gamePopup = POPUP_HUD;
                        break;
                /*case 1:
                        *gamePopup = POPUP_OPTIONS;
                        break;*/
                case 1: //case 2:
                        int err = World_save(world);
                        if (err) {
                                gameLoop_error("Could not save world");
                                return;
                        }
                        World_wipe(world);
                        *gameState = STATE_TITLE;
                        break;
                }
                selection = 0;
        }
}

/* popup_options
 * Shows an options screen.
 */
void popup_options (SDL_Renderer *renderer, Inputs *inputs, int *gamePopup) {
        vpadReadInput();

        tblack(renderer);
        SDL_Rect bg = { 0, 0, BUFFER_W, BUFFER_H };
        SDL_RenderFillRect(renderer, &bg);

        drawMenuTitle(renderer, "Options");

        if (menu_optionsMain(renderer, inputs)) {
                *gamePopup = 1;
        }
}

#ifndef small
/* popup_debugTools
 * Shows a menu listing advanced debug tools.
 */
void popup_debugTools (SDL_Renderer *renderer, Inputs *inputs, int *gamePopup) {
        vpadReadInput();
        (void)(inputs);

        static int selection = 0;

        if (vpadButtonPressed(VPAD_BUTTON_UP)) {
                selection--;
                if (selection < 0) selection = 3;
        }
        if (vpadButtonPressed(VPAD_BUTTON_DOWN)) {
                selection++;
                if (selection > 3) selection = 0;
        }
        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                *gamePopup = POPUP_HUD;
                selection = 0;
                return;
        }

        tblack(renderer);
        SDL_Rect bg = { 0, 0, BUFFER_W, BUFFER_H };
        SDL_RenderFillRect(renderer, &bg);

        drawMenuTitle(renderer, "Debug Tools");

        drawMenuButton(renderer, "Chunk Peek", BUFFER_HALF_W - 64, 30, 128, selection == 0);
        drawMenuButton(renderer, "All Chunks", BUFFER_HALF_W - 64, 52, 128, selection == 1);
        drawMenuButton(renderer, "World Overview", BUFFER_HALF_W - 64, 74, 128, selection == 2);
        drawMenuButton(renderer, "Done", BUFFER_HALF_W - 64, 96, 128, selection == 3);

        if (vpadButtonPressed(VPAD_BUTTON_A)) {
                switch (selection) {
                case 0:
                        *gamePopup = POPUP_CHUNK_PEEK;
                        break;
                case 1:
                        *gamePopup = POPUP_ROLL_CALL;
                        break;
                case 2:
                        *gamePopup = POPUP_OVERVIEW;
                        break;
                case 3:
                        *gamePopup = POPUP_HUD;
                        break;
                }
                selection = 0;
        }
}

/* popup_chunkPeek
 * Shows a 3D map of the current chunk.
 */
void popup_chunkPeek (
        SDL_Renderer *renderer, Inputs *inputs, World *world,
        int *gamePopup,
        Player *player
) {
        vpadReadInput();
        (void)(inputs);

        static int chunkPeekRYMax = 0;

        int chunkPeekRX,
            chunkPeekRY,
            chunkPeekRZ,
            chunkPeekColor;

        Chunk *debugChunk;
        char chunkPeekText[][32] = {
                "coordHash: ",
                "loaded: "
        };

        if (vpadButtonHeld(VPAD_BUTTON_UP)) {
                chunkPeekRYMax = nmod(chunkPeekRYMax - 1, 64);
        }
        if (vpadButtonHeld(VPAD_BUTTON_DOWN)) {
                chunkPeekRYMax = nmod(chunkPeekRYMax + 1, 64);
        }

        debugChunk = chunkLookup (
                world,
                (int)player->pos.x,
                (int)player->pos.y,
                (int)player->pos.z
        );

        white(renderer);
        if (debugChunk != NULL) {
                strnum(chunkPeekText[0], 11, debugChunk->coordHash);
                strnum(chunkPeekText[1], 8,  debugChunk->loaded);
                for (int i = 0; i < 2; i++) {
                        drawStr(renderer, chunkPeekText[i], 0, i << 3);
                }

                white(renderer);
                SDL_RenderDrawLine(renderer, 128, chunkPeekRYMax, 191, chunkPeekRYMax);

                for (
                        chunkPeekRY = 64;
                        chunkPeekRY >= chunkPeekRYMax;
                        chunkPeekRY--
                ) for (
                        chunkPeekRX = 0;
                        chunkPeekRX < 64;
                        chunkPeekRX++
                ) for (
                        chunkPeekRZ = 0;
                        chunkPeekRZ < 63;
                        chunkPeekRZ++
                ) {
                        Block currentBlock = debugChunk->blocks [
                                chunkPeekRX +
                                (chunkPeekRY << 6) +
                                (chunkPeekRZ << 12)];

                        chunkPeekColor = textures [
                                currentBlock * 256 * 3 + 6 * 16];

                        if (chunkPeekColor) {
                                int alpha = 255;
                                if (currentBlock == BLOCK_WATER) {
                                        alpha = 64;
                                }

                                SDL_SetRenderDrawColor (
                                        renderer,
                                        (chunkPeekColor >> 16 & 0xFF),
                                        (chunkPeekColor >> 8 & 0xFF),
                                        (chunkPeekColor & 0xFF),
                                        alpha);

                                SDL_RenderDrawPoint (
                                        renderer,
                                        chunkPeekRX + 128,
                                        chunkPeekRY + chunkPeekRZ);

                                SDL_SetRenderDrawColor (
                                        renderer, 0, 0, 0, 64);

                                SDL_RenderDrawPoint (
                                        renderer,
                                        chunkPeekRX + 128,
                                        chunkPeekRY + chunkPeekRZ + 1);
                        }
                }
        } else {
                drawStr(renderer, "Chunk not found", 0, 0);
        }

        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                *gamePopup = POPUP_ADVANCED_DEBUG;
        }
}

void popup_rollCall (
        SDL_Renderer *renderer, Inputs *inputs, World *world,
        int *gamePopup
) {
        vpadReadInput();
        (void)(inputs);

        static int scroll = 0;

        if (vpadButtonHeld(VPAD_BUTTON_UP)) {
                scroll++;
        }
        if (vpadButtonHeld(VPAD_BUTTON_DOWN)) {
                scroll--;
        }

        if (scroll > 0) scroll = 0;
        if (scroll < 1 - CHUNKARR_SIZE) scroll = 1 - CHUNKARR_SIZE;

        white(renderer);
        drawStr(renderer, "x    y    z   stmp    hash", 8, 10);

        for (int index = 0; index < CHUNKARR_SIZE; index++) {
                Chunk *chunk = &world->chunk[index];
                char chunkDescription[32];
                white(renderer);

                int topMargin = 28;
                int y = (index + scroll) * 8 + topMargin;
                if (y < topMargin || y >= BUFFER_H) continue;

                snprintf(chunkDescription, 32, "%i", chunk->center.x - 32);
                drawStr(renderer, chunkDescription, 0,   y);
                snprintf(chunkDescription, 32, "%i", chunk->center.y - 32);
                drawStr(renderer, chunkDescription, 24,  y);
                snprintf(chunkDescription, 32, "%i", chunk->center.z - 32);
                drawStr(renderer, chunkDescription, 48,  y);

                snprintf(chunkDescription, 32, "#%i", chunk->loaded);
                drawStr(renderer, chunkDescription, 72,  y);
                snprintf(chunkDescription, 32, "%016x", chunk->coordHash);
                drawStr(renderer, chunkDescription, 96,  y);
        }

        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                *gamePopup = POPUP_ADVANCED_DEBUG;
        }
}

void popup_overview (
        SDL_Renderer *renderer, Inputs *inputs, World *world,
        int *gamePopup
) {
        vpadReadInput();
        (void)(inputs);
        (void)(world);

        int worldEndingBound   = CHUNK_SIZE * (CHUNKARR_RAD + 1);
        int worldStartingBound = CHUNK_SIZE * CHUNKARR_RAD * -1;
        for (int y = worldEndingBound; y > worldStartingBound; y -= 4)
        for (int x = worldStartingBound; x < worldEndingBound; x += 4)
        for (int z = worldStartingBound; z < worldEndingBound; z += 4) {
                int projectX = (x - z) / 4;
                int projectY = ((x + z) / 2 + y) / 4;

                Block currentBlock = World_getBlock(world, x, y, z);
                int color;
                int alpha = 255;

                if (currentBlock < NUMBER_OF_BLOCKS) {
                        color = textures[currentBlock * 256 * 3 + 6 * 16];
                } else {
                        color = 0xFF0000;
                        alpha = 0;
                }

                if (color != 0) {
                        if (currentBlock == BLOCK_WATER) {
                                alpha = 64;
                        }

                        SDL_SetRenderDrawColor (
                                renderer,
                                (color >> 16 & 0xFF),
                                (color >> 8 & 0xFF),
                                (color & 0xFF),
                                alpha);

                        SDL_RenderDrawPoint (
                                renderer,
                                projectX + BUFFER_HALF_W,
                                projectY + 32);
                }
        }

        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                *gamePopup = POPUP_ADVANCED_DEBUG;
        }
}
#endif

/* menu_optionsMain
 * Options menu navigable with D-pad.
 */
static int menu_optionsMain (SDL_Renderer *renderer, Inputs *inputs) {
        (void)(inputs);
        static int selection = 0;
        static int page = 0;

        int maxSelection = 3; // Options per page + Done button

        if (vpadButtonPressed(VPAD_BUTTON_UP)) {
                selection--;
                if (selection < 0) selection = maxSelection;
        }
        if (vpadButtonPressed(VPAD_BUTTON_DOWN)) {
                selection++;
                if (selection > maxSelection) selection = 0;
        }
        if (vpadButtonPressed(VPAD_BUTTON_LEFT) && selection == maxSelection) {
                page--;
                page = nmod(page, 2);
        }
        if (vpadButtonPressed(VPAD_BUTTON_RIGHT) && selection == maxSelection) {
                page++;
                page = nmod(page, 2);
        }

        // Page navigation at top
        char pageText[16];
        snprintf(pageText, 16, "< Page %i >", page + 1);
        int isPageSelected = (selection == maxSelection);
        if (isPageSelected) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
                SDL_Rect bg = { BUFFER_HALF_W - 40, 30, 80, 18 };
                SDL_RenderFillRect(renderer, &bg);
                drawMenuCursor(renderer, BUFFER_HALF_W - 52, 34);
        }
        white(renderer);
        drawStr(renderer, pageText, BUFFER_HALF_W - 32, 34);

        switch (page) {
        case 0:
                drawMenuButton(renderer, options.username.buffer[0] ? options.username.buffer : "Username: (none)",
                        BUFFER_HALF_W - 64, 52, 128, selection == 0);

                static char *trapMouseTexts[] = {
                        "Capture Mouse: OFF",
                        "Capture Mouse: ON"
                };
                drawMenuButton(renderer, trapMouseTexts[options.trapMouse],
                        BUFFER_HALF_W - 64, 74, 128, selection == 1);

                if (selection == 2) {
                        drawMenuButton(renderer, "Done", BUFFER_HALF_W - 64, 96, 128, 1);
                } else {
                        drawMenuButton(renderer, "Done", BUFFER_HALF_W - 64, 96, 128, 0);
                }
                break;
        case 1:
                ;static char drawDistanceText[20] = "Draw distance: ";
                strnum(drawDistanceText + 15, 3, options.drawDistance);
                drawMenuButton(renderer, drawDistanceText,
                        BUFFER_HALF_W - 64, 52, 128, selection == 0);

                static char *fovTexts[] = {
                        "FOV: Low",
                        "FOV: Medium",
                        "FOV: High",
                        "FOV: ?"
                };
                char *fovText = NULL;
                switch ((int)options.fov) {
                        default:  fovText = fovTexts[3]; break;
                        case 60:  fovText = fovTexts[2]; break;
                        case 90:  fovText = fovTexts[1]; break;
                        case 140: fovText = fovTexts[0]; break;
                }
                drawMenuButton(renderer, fovText,
                        BUFFER_HALF_W - 64, 74, 128, selection == 1);

                if (selection == 2) {
                        drawMenuButton(renderer, "Done", BUFFER_HALF_W - 64, 96, 128, 1);
                } else {
                        drawMenuButton(renderer, "Done", BUFFER_HALF_W - 64, 96, 128, 0);
                }
                break;
        }

        // Handle A button presses
        if (vpadButtonPressed(VPAD_BUTTON_A)) {
                if (selection == maxSelection) {
                        // Done button
                        int err = options_save();
                        if (err) {
                                gameLoop_error("Could not save options");
                        }
                        page = 0;
                        selection = 0;
                        return 1;
                }

                switch (page) {
                case 0:
                        switch (selection) {
                        case 0:
                                // Username - cycle through some presets for console
                                if (strcmp(options.username.buffer, "Player") == 0) {
                                        strncpy(options.username.buffer, "Steve", 16);
                                } else if (strcmp(options.username.buffer, "Steve") == 0) {
                                        strncpy(options.username.buffer, "Alex", 16);
                                } else {
                                        strncpy(options.username.buffer, "Player", 16);
                                }
                                break;
                        case 1:
                                options.trapMouse = !options.trapMouse;
                                break;
                        }
                        break;
                case 1:
                        switch (selection) {
                        case 0:
                                switch (options.drawDistance) {
                                case 20:  options.drawDistance = 32; break;
                                case 32:  options.drawDistance = 64; break;
                                case 64:  options.drawDistance = 96; break;
                                case 96:  options.drawDistance = 128; break;
                                default:  options.drawDistance = 20; break;
                                }
                                break;
                        case 1:
                                switch ((int)options.fov) {
                                case 60:  options.fov = 140; break;
                                case 90:  options.fov = 60;  break;
                                default:  options.fov = 90;  break;
                                }
                                break;
                        }
                        break;
                }
        }

        if (vpadButtonPressed(VPAD_BUTTON_B)) {
                int err = options_save();
                if (err) {
                        gameLoop_error("Could not save options");
                }
                page = 0;
                selection = 0;
                return 1;
        }

        return 0;
}