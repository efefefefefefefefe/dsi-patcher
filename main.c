/*
 * TrollWare - A Multi-Stage Troll Game for Nintendo DS
 * Built with libnds + devkitARM
 *
 * Stages:
 *   1. Fake Loading Screen (freezes at 99%, fake error on input)
 *   2. Inverted Maze     (X/Y controls are swapped)
 *   3. Scrambled Quiz    (choices scramble when stylus nears correct answer)
 *
 * Bonus:
 *   - Secret "rickroll" ASCII art on the top screen
 *   - Random fake "system warnings" injected during Stage 2
 *   - Fake save corruption screen between Stage 2 and Stage 3
 *   - Endgame trollface ASCII art
 */

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────
   State Machine
   ───────────────────────────────────────────── */
typedef enum {
    STATE_LOADING,
    STATE_ERROR,
    STATE_MAZE,
    STATE_FAKE_CORRUPT,
    STATE_QUIZ,
    STATE_WIN
} GameState;

/* ─────────────────────────────────────────────
   Console handles (top = sub, bottom = main)
   ───────────────────────────────────────────── */
static PrintConsole topScreen, bottomScreen;

/* ─────────────────────────────────────────────
   Timing / frame counter
   ───────────────────────────────────────────── */
static u32 frameCount = 0;

/* ═══════════════════════════════════════════════════
   UTILITIES
   ═══════════════════════════════════════════════════ */

static void clearBottom(void) {
    consoleSelect(&bottomScreen);
    consoleClear();
}

static void clearTop(void) {
    consoleSelect(&topScreen);
    consoleClear();
}

/* Print centered text on the currently selected console (32 cols wide) */
static void printCentered(int row, const char *text) {
    int len = strlen(text);
    int col = (32 - len) / 2;
    if (col < 0) col = 0;
    iprintf("\x1b[%d;%dH%s", row, col, text);
}

/* Simple LCG pseudo-random (no srand needed for trolling purposes) */
static u32 trollRand(void) {
    static u32 seed = 0xDEADBEEF;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

/* ═══════════════════════════════════════════════════
   TOP SCREEN: RICKROLL ASCII ART
   ═══════════════════════════════════════════════════ */
static void drawTopScreenArt(void) {
    consoleSelect(&topScreen);
    consoleClear();

    /* Colour codes: \x1b[3Xm = foreground colour */
    iprintf("\x1b[0;0H");
    iprintf("\x1b[33m"); /* yellow */
    iprintf("  +---------------------------------+\n");
    iprintf("  |   ** NDS SYSTEM DIAGNOSTICS **  |\n");
    iprintf("  +---------------------------------+\n");
    iprintf("\x1b[37m\n"); /* white */

    /* Tiny ASCII stick figure holding a sign */
    iprintf("         \\O/   WE'RE NO STRANGERS\n");
    iprintf("          |    TO LOVE...\n");
    iprintf("         / \\   \n\n");

    iprintf("\x1b[36m"); /* cyan */
    iprintf("  Never gonna give you up,\n");
    iprintf("  Never gonna let you down,\n");
    iprintf("  Never gonna run around\n");
    iprintf("     and desert you~ :)\n");
    iprintf("\x1b[37m\n");
    iprintf("  [NDS FW v3.7.2 - Build 20070401]\n");
    iprintf("  CPU Temp : ???C  Battery: LOL%%\n");
    iprintf("  WiFi     : Definitely connected\n");
    iprintf("\x1b[33m");
    iprintf("  Touch lower screen to continue.\n");
    iprintf("\x1b[37m");
}

/* ═══════════════════════════════════════════════════
   STAGE 1 – FAKE LOADING SCREEN
   ═══════════════════════════════════════════════════ */
#define LOAD_FREEZE_FRAME 120   /* freeze after ~2 sec */
#define LOAD_SPEED        2     /* pixels per frame     */

static int  loadProgress   = 0;
static bool loadFrozen     = false;
static int  blinkTimer     = 0;

static void initLoading(void) {
    clearBottom();
    loadProgress = 0;
    loadFrozen   = false;
    blinkTimer   = 0;
}

/* Draw the loading bar (28 chars wide inside brackets) */
static void drawLoadBar(void) {
    consoleSelect(&bottomScreen);

    iprintf("\x1b[5;2H\x1b[36m*** TROLLWARE v1.0 ***\x1b[37m");
    iprintf("\x1b[7;2H  Initialising subsystems...");
    iprintf("\x1b[9;2H  Loading assets...");
    iprintf("\x1b[11;2H  Calibrating troll engines...\x1b[37m");

    /* Bar: 28 chars = 0-100% */
    int filled = (loadProgress * 28) / 100;
    iprintf("\x1b[14;1H [");
    for (int i = 0; i < 28; i++) {
        if (i < filled)
            iprintf("\x1b[32m#\x1b[37m");
        else
            iprintf("-");
    }
    iprintf("] %3d%%", loadProgress);

    if (loadFrozen) {
        /* Blink "PLEASE WAIT" */
        blinkTimer++;
        if ((blinkTimer / 30) & 1)
            iprintf("\x1b[16;4H\x1b[33m!! PLEASE WAIT !!\x1b[37m");
        else
            iprintf("\x1b[16;4H                  ");

        iprintf("\x1b[20;0H\x1b[31m  [Any button] Fake Error Screen\x1b[37m");
        iprintf("\x1b[21;0H\x1b[32m  [START]      Skip to Maze\x1b[37m");
    }
}

/* Returns next state */
static GameState updateLoading(u32 keys, touchPosition *tp) {
    (void)tp;

    if (!loadFrozen) {
        if ((frameCount % 3) == 0 && loadProgress < 99) {
            loadProgress += LOAD_SPEED;
            if (loadProgress >= 99) {
                loadProgress = 99;
                loadFrozen   = true;
            }
        }
    } else {
        if (keys & KEY_START)   return STATE_MAZE;
        if (keys & ~KEY_START)  return STATE_ERROR; /* any other key */
        /* Also trigger on touch */
        if (keys & KEY_TOUCH)   return STATE_ERROR;
    }
    return STATE_LOADING;
}

/* ═══════════════════════════════════════════════════
   STAGE 1b – FAKE ERROR SCREEN
   ═══════════════════════════════════════════════════ */
static int errorTimer = 0;

static const char *errorMessages[] = {
    "KERNEL_TROLL_EXCEPTION",
    "ILLEGAL_FUN_DETECTED",
    "TOO_MUCH_SKILL_FAULT",
    "UNEXPECTED_TALENT_0x00B00B5",
    "NULL_PATIENCE_VIOLATION",
    "STACK_OVERFLOW_OF_COPE",
};
#define NUM_ERRORS 6

static void initError(void) {
    clearBottom();
    errorTimer = 0;
    int idx = trollRand() % NUM_ERRORS;

    consoleSelect(&bottomScreen);
    iprintf("\x1b[0;0H");
    iprintf("\x1b[41m"); /* red background */
    iprintf("################################\n");
    iprintf("#   !! FATAL SYSTEM ERROR !!   #\n");
    iprintf("################################\n");
    iprintf("\x1b[40m\x1b[31m\n"); /* black bg, red fg */
    iprintf(" Error Code : 0x%08lX\n", (unsigned long)(trollRand() & 0xFFFFFFFF));
    iprintf(" Module     : %s\n", errorMessages[idx]);
    iprintf(" Thread     : trolld.arm9\n");
    iprintf(" Memory     : %lu KB free\n", (unsigned long)(trollRand() % 256));
    iprintf("\n\x1b[37m");
    iprintf(" The system has encountered an\n");
    iprintf(" error it cannot handle.\n\n");
    iprintf(" Would you like to:\n\n");
    iprintf("  [A] Send error report (lol)\n");
    iprintf("  [B] Format console (?!)\n");
    iprintf("  [START] Accept destiny -> Maze\n");
    iprintf("\n\x1b[33m");
    iprintf(" Rebooting in ...\x1b[37m");
}

static GameState updateError(u32 keys) {
    errorTimer++;
    consoleSelect(&bottomScreen);

    int secs = 5 - (errorTimer / 60);
    if (secs < 0) secs = 0;
    iprintf("\x1b[20;17H\x1b[31m%d sec\x1b[37m", secs);

    if (keys & KEY_START)  return STATE_MAZE;
    if (errorTimer > 360)  return STATE_MAZE; /* auto-advance after 6 sec */
    return STATE_ERROR;
}

/* ═══════════════════════════════════════════════════
   STAGE 2 – INVERTED MAZE
   ═══════════════════════════════════════════════════ */

/*
 * Grid is 10 cols x 8 rows drawn on the bottom screen.
 * Each cell is 3 chars wide, 2 rows tall in text coords.
 * Screen is 32 cols x 24 rows.
 * Grid origin (top-left cell 0,0) at text col 1, row 2.
 */

#define MAZE_COLS  10
#define MAZE_ROWS  8
#define CELL_W     3
#define CELL_H     2

/* Pixel touch range for each cell (bottom screen = 256x192 px) */
#define CELL_PX_W  (256 / MAZE_COLS)    /* 25 */
#define CELL_PX_H  (192 / MAZE_ROWS)    /* 24 */

typedef struct { int x, y; } Vec2;

/* Simple maze wall map: 1=wall, 0=open */
static const u8 mazeWalls[MAZE_ROWS][MAZE_COLS] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 1},
    {1, 1, 0, 1, 0, 0, 1, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

static Vec2 playerPos;
static Vec2 goalPos;
static bool stage3Unlocked = false;
static int  warningTimer   = 0;
static bool showWarning    = false;
static int  warningIdx     = 0;

static const char *fakeWarnings[] = {
    "Warning: Troll levels critical!",
    "Alert: Fun.exe has crashed",
    "Notice: IQ dropped below 50",
    "Caution: Skill not found",
    "Warning: Git blame -> YOU",
};
#define NUM_WARNINGS 5

static void initMaze(void) {
    clearBottom();
    clearTop();
    drawTopScreenArt();

    playerPos.x  = 1; playerPos.y = 1;
    goalPos.x    = 8; goalPos.y  = 6;
    stage3Unlocked = false;
    warningTimer   = 0;
    showWarning    = false;
}

static void drawMaze(void) {
    consoleSelect(&bottomScreen);
    consoleClear();

    iprintf("\x1b[0;1H\x1b[35m-- INVERTED MAZE (stylus INVERTED) --\x1b[37m");

    for (int row = 0; row < MAZE_ROWS; row++) {
        for (int col = 0; col < MAZE_COLS; col++) {
            int tx = 1 + col * 1;   /* compact: 1 char per cell for 32-col fit */
            int ty = 2 + row;

            iprintf("\x1b[%d;%dH", ty, tx);

            if (mazeWalls[row][col]) {
                iprintf("\x1b[34m#\x1b[37m");
            } else if (col == playerPos.x && row == playerPos.y) {
                iprintf("\x1b[32m@\x1b[37m");
            } else if (col == goalPos.x && row == goalPos.y) {
                iprintf("\x1b[33mX\x1b[37m");
            } else {
                iprintf(".");
            }
        }
    }

    iprintf("\x1b[11;0H\x1b[36mPlayer\x1b[37m: (%d,%d)  \x1b[33mGoal\x1b[37m: (%d,%d)",
            playerPos.x, playerPos.y, goalPos.x, goalPos.y);
    iprintf("\x1b[12;0H\x1b[31mTip: Touch where you DON'T want to go!\x1b[37m");
    iprintf("\x1b[13;0H@ = You   X = Goal (tap X to... fail?)");

    if (showWarning) {
        iprintf("\x1b[15;0H\x1b[41m%-32s\x1b[40m\x1b[37m", fakeWarnings[warningIdx]);
    } else {
        iprintf("\x1b[15;0H                                ");
    }

    if (stage3Unlocked) {
        iprintf("\x1b[17;0H\x1b[32m** Goal reached! Press START for Quiz **\x1b[37m");
    }

    iprintf("\x1b[23;0H\x1b[33mSTART = next stage (if unlocked)\x1b[37m");
}

/* Convert touch pixel -> maze cell, then INVERT */
static Vec2 touchToMazeInverted(int px, int py) {
    /* Raw cell */
    int rawCol = px / CELL_PX_W;
    int rawRow = py / CELL_PX_H;
    /* Invert within grid bounds */
    Vec2 r;
    r.x = (MAZE_COLS - 1) - rawCol;
    r.y = (MAZE_ROWS - 1) - rawRow;
    if (r.x < 0) r.x = 0;
    if (r.y < 0) r.y = 0;
    if (r.x >= MAZE_COLS) r.x = MAZE_COLS - 1;
    if (r.y >= MAZE_ROWS) r.y = MAZE_ROWS - 1;
    return r;
}

static GameState updateMaze(u32 keys, touchPosition *tp) {
    /* Random fake warning every ~5 seconds */
    warningTimer++;
    if (warningTimer > 300) {
        warningTimer = 0;
        showWarning  = !showWarning;
        if (showWarning)
            warningIdx = trollRand() % NUM_WARNINGS;
    }

    if (keys & KEY_TOUCH) {
        Vec2 dest = touchToMazeInverted(tp->px, tp->py);

        /* Check if touching the GOAL cell (non-inverted coords match goal) */
        int rawCol = tp->px / CELL_PX_W;
        int rawRow = tp->py / CELL_PX_H;

        if (rawCol == goalPos.x && rawRow == goalPos.y) {
            /* Player touched the goal — move goal to start (troll!) */
            goalPos.x = 1; goalPos.y = 1;
            stage3Unlocked = true;
        }

        /* Move player to INVERTED destination if not a wall */
        if (!mazeWalls[dest.y][dest.x]) {
            playerPos = dest;
        }
    }

    if (stage3Unlocked && (keys & KEY_START))
        return STATE_FAKE_CORRUPT;

    return STATE_MAZE;
}

/* ═══════════════════════════════════════════════════
   STAGE 2.5 – FAKE SAVE CORRUPTION
   ═══════════════════════════════════════════════════ */
static int corruptTimer = 0;

static void initFakeCorrupt(void) {
    clearBottom();
    corruptTimer = 0;
    consoleSelect(&bottomScreen);

    iprintf("\x1b[5;3H\x1b[33m*** SAVE DATA WARNING ***\x1b[37m\n\n");
    iprintf("  Slot 1: [OK]   27 hrs\n");
    iprintf("  Slot 2: [OK]   03 hrs\n");
    iprintf("  Slot 3: [");
    iprintf("\x1b[31mCORRUPT\x1b[37m] !!!\n\n");
    iprintf("  Backing up save data");
    iprintf("\x1b[31m (please do not\n");
    iprintf("  turn off the power)\x1b[37m\n\n");
    iprintf("  Progress: [--------------------]\n");
    iprintf("\n  Just kidding lol.\n");
    iprintf("  There is no save.\n");
    iprintf("  \x1b[32mContinuing in 3 sec...\x1b[37m\n");
}

static GameState updateFakeCorrupt(u32 keys) {
    (void)keys;
    corruptTimer++;

    /* Animate the fake progress bar */
    int filled = (corruptTimer * 20) / 180;
    if (filled > 20) filled = 20;

    consoleSelect(&bottomScreen);
    iprintf("\x1b[14;13H");
    for (int i = 0; i < 20; i++)
        iprintf(i < filled ? "\x1b[32m=\x1b[37m" : "-");

    if (corruptTimer > 180) return STATE_QUIZ;
    return STATE_FAKE_CORRUPT;
}

/* ═══════════════════════════════════════════════════
   STAGE 3 – SCRAMBLED QUIZ
   ═══════════════════════════════════════════════════ */

#define QUIZ_CHOICES 4

/*
 * Layout: 4 choice boxes arranged 2x2.
 * Each box occupies a quadrant of the bottom screen (256x192).
 * Correct answer index is stored; when stylus gets close it scrambles.
 */

static const char *quizQuestion  = "What is 2 + 2?";
/* Choices and labels cycle via scramble permutation */
static const char *quizChoices[QUIZ_CHOICES] = {
    "A) Fish",
    "B) 4",      /* Correct answer, initially index 1 */
    "C) Yes",
    "D) Purple",
};

/* Current display order: choiceOrder[display_slot] = original_index */
static int  choiceOrder[QUIZ_CHOICES] = {0, 1, 2, 3};
static int  correctOriginal           = 1; /* "B) 4" is correct */
static int  scrambleCount             = 0;
static bool quizSolved                = false;

/* Box pixel boundaries for 4 quadrants */
typedef struct { int x1, y1, x2, y2; } Box;
static const Box choiceBoxes[QUIZ_CHOICES] = {
    {  0,  96, 128, 192 },   /* bottom-left  (slot 0) */
    {128,  96, 256, 192 },   /* bottom-right (slot 1) */
    {  0,   0, 128,  96 },   /* top-left     (slot 2) */
    {128,   0, 256,  96 },   /* top-right    (slot 3) */
};

/* Row/col positions for text inside each quadrant */
static const int boxTextRow[QUIZ_CHOICES] = {14, 14, 3, 3};
static const int boxTextCol[QUIZ_CHOICES] = {1, 17, 1, 17};

static void scrambleChoices(void) {
    /* Fisher-Yates on choiceOrder */
    for (int i = QUIZ_CHOICES - 1; i > 0; i--) {
        int j = trollRand() % (i + 1);
        int tmp = choiceOrder[i];
        choiceOrder[i] = choiceOrder[j];
        choiceOrder[j] = tmp;
    }
    scrambleCount++;
}

/* Find which display slot currently holds the correct answer */
static int correctDisplaySlot(void) {
    for (int s = 0; s < QUIZ_CHOICES; s++)
        if (choiceOrder[s] == correctOriginal) return s;
    return -1;
}

/* Distance from touch point to centre of a box */
static int boxDist(int px, int py, const Box *b) {
    int cx = (b->x1 + b->x2) / 2;
    int cy = (b->y1 + b->y2) / 2;
    int dx = px - cx, dy = py - cy;
    return dx*dx + dy*dy; /* squared, no sqrt needed */
}

static void initQuiz(void) {
    clearBottom();
    choiceOrder[0] = 0; choiceOrder[1] = 1;
    choiceOrder[2] = 2; choiceOrder[3] = 3;
    scrambleCount  = 0;
    quizSolved     = false;
}

static void drawQuiz(void) {
    consoleSelect(&bottomScreen);
    consoleClear();

    iprintf("\x1b[1;2H\x1b[36m*** TROLLWARE FINAL QUIZ ***\x1b[37m");
    iprintf("\x1b[2;2H\x1b[33m%s\x1b[37m", quizQuestion);

    /* Draw a dividing cross */
    for (int r = 0; r < 24; r++) iprintf("\x1b[%d;16H|", r + 4);
    for (int c = 0; c < 32; c++) iprintf("\x1b[12;%dH-", c);

    /* Draw each choice in its current slot */
    for (int slot = 0; slot < QUIZ_CHOICES; slot++) {
        int origIdx = choiceOrder[slot];
        iprintf("\x1b[%d;%dH\x1b[37m%s          ",
                boxTextRow[slot], boxTextCol[slot],
                quizChoices[origIdx]);
    }

    iprintf("\x1b[22;0H\x1b[31mScrambles so far: %d\x1b[37m", scrambleCount);
    iprintf("\x1b[23;0H\x1b[33mTap the correct answer (good luck!)\x1b[37m");
}

static GameState updateQuiz(u32 keys, touchPosition *tp) {
    (void)keys;

    if (!quizSolved && (keys & KEY_TOUCH)) {
        int px = tp->px, py = tp->py;

        /* Check proximity to correct-answer box */
        int correct_slot = correctDisplaySlot();
        if (correct_slot >= 0) {
            int d = boxDist(px, py, &choiceBoxes[correct_slot]);
            if (d < 2500) { /* ~50px radius squared */
                scrambleChoices();
            }
        }

        /* Check if player tapped the WRONG box (troll: any tap scrambles) */
        /* Actually check if they tapped the correct slot exactly */
        for (int slot = 0; slot < QUIZ_CHOICES; slot++) {
            const Box *b = &choiceBoxes[slot];
            if (px >= b->x1 && px < b->x2 && py >= b->y1 && py < b->y2) {
                if (choiceOrder[slot] == correctOriginal) {
                    /* Correct tap! But wait... scramble one last time :) */
                    if (scrambleCount < 3) {
                        scrambleChoices();
                    } else {
                        quizSolved = true;
                        return STATE_WIN;
                    }
                } else {
                    /* Wrong answer – show taunt */
                    consoleSelect(&bottomScreen);
                    iprintf("\x1b[20;0H\x1b[31mNope! Scrambling for fun!  \x1b[37m");
                    scrambleChoices();
                }
                break;
            }
        }
    }
    return STATE_QUIZ;
}

/* ═══════════════════════════════════════════════════
   WIN SCREEN – TROLLFACE ASCII
   ═══════════════════════════════════════════════════ */
static int winTimer = 0;

static void initWin(void) {
    clearBottom();
    clearTop();
    winTimer = 0;

    consoleSelect(&topScreen);
    iprintf("\x1b[1;8H\x1b[33m*** U GOT TROLLED ***\x1b[37m\n\n");
    iprintf("         .------.\n");
    iprintf("        /  o  o  \\\n");
    iprintf("       |    __    |\n");
    iprintf("        \\  (__)  /\n");
    iprintf("    .---'--------'---.\n");
    iprintf("   /  PROBLEM OFFICER? \\\n");
    iprintf("  /____________________\\\n\n");
    iprintf("\x1b[36m  Congratulations! You survived\n");
    iprintf("  TrollWare version 1.0\n\n");
    iprintf("\x1b[37m  Final Score: -999,999 points\n");
    iprintf("  Time wasted: All of it\n");
    iprintf("  Dignity remaining: 0%%\n");

    consoleSelect(&bottomScreen);
    iprintf("\x1b[5;3H\x1b[32mYou actually did it.\x1b[37m\n\n");
    iprintf("  The loading bar was fake.\n");
    iprintf("  The maze was inverted.\n");
    iprintf("  The quiz was rigged.\n\n");
    iprintf("  \x1b[33mAnd yet here you are.\x1b[37m\n\n");
    iprintf("  True gamer. Much respect.\n\n");
    iprintf("  \x1b[35m~ TrollWare by devkitARM ~\x1b[37m\n\n");
    iprintf("  [START] to restart the madness");
}

static GameState updateWin(u32 keys) {
    winTimer++;
    if (keys & KEY_START) return STATE_LOADING;
    return STATE_WIN;
}

/* ═══════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════ */
int main(void) {
    /* Init video / consoles */
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true,  true);
    consoleInit(&topScreen,    3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    GameState state     = STATE_LOADING;
    GameState prevState = (GameState)-1;

    while (1) {
        swiWaitForVBlank();
        frameCount++;

        scanKeys();
        u32 keys = keysDown();

        touchPosition tp;
        touchRead(&tp);

        /* Re-init on state change */
        if (state != prevState) {
            prevState = state;
            switch (state) {
                case STATE_LOADING:      initLoading();      drawTopScreenArt(); break;
                case STATE_ERROR:        initError();        break;
                case STATE_MAZE:         initMaze();         break;
                case STATE_FAKE_CORRUPT: initFakeCorrupt();  break;
                case STATE_QUIZ:         initQuiz();         break;
                case STATE_WIN:          initWin();          break;
            }
        }

        /* Update & draw */
        switch (state) {
            case STATE_LOADING:
                state = updateLoading(keys, &tp);
                drawLoadBar();
                break;

            case STATE_ERROR:
                state = updateError(keys);
                break;

            case STATE_MAZE:
                state = updateMaze(keys, &tp);
                drawMaze();
                break;

            case STATE_FAKE_CORRUPT:
                state = updateFakeCorrupt(keys);
                break;

            case STATE_QUIZ:
                state = updateQuiz(keys, &tp);
                drawQuiz();
                break;

            case STATE_WIN:
                state = updateWin(keys);
                break;
        }
    }

    return 0;
}
