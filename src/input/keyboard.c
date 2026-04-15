/*
 * SPEmulator — Keyboard
 * ZX Spectrum matrix mapping from SDL scancodes.
 *
 * ZX Spectrum keyboard matrix (active low, 0 = pressed):
 *   Row 0 (A8): SHIFT Z X C V          (half-row 0xFEFE)
 *   Row 1 (A9): A S D F G              (half-row 0xFDFE)
 *   Row 2 (A10): Q W E R T             (half-row 0xFBFE)
 *   Row 3 (A11): 1 2 3 4 5             (half-row 0xF7FE)
 *   Row 4 (A12): 0 9 8 7 6             (half-row 0xEFFE)
 *   Row 5 (A13): P O I U Y             (half-row 0xDFFE)
 *   Row 6 (A14): ENTER L K J H         (half-row 0xBFFE)
 *   Row 7 (A15): SPACE SYM M N B       (half-row 0x7FFE)
 */
#include "input/keyboard.h"
#include "machine.h"
#include <SDL3/SDL.h>
#include <string.h>

/* Mapping entry: SDL scancode -> ZX row, bit */
typedef struct {
    SDL_Scancode scancode;
    u8 row;
    u8 bit;  /* 0-4, active low */
} zx_keymap_t;

static const zx_keymap_t keymap[] = {
    /* Row 0: Shift Z X C V */
    { SDL_SCANCODE_LSHIFT,  0, 0 },
    { SDL_SCANCODE_RSHIFT,  0, 0 },
    { SDL_SCANCODE_Z,       0, 1 },
    { SDL_SCANCODE_X,       0, 2 },
    { SDL_SCANCODE_C,       0, 3 },
    { SDL_SCANCODE_V,       0, 4 },

    /* Row 1: A S D F G */
    { SDL_SCANCODE_A,       1, 0 },
    { SDL_SCANCODE_S,       1, 1 },
    { SDL_SCANCODE_D,       1, 2 },
    { SDL_SCANCODE_F,       1, 3 },
    { SDL_SCANCODE_G,       1, 4 },

    /* Row 2: Q W E R T */
    { SDL_SCANCODE_Q,       2, 0 },
    { SDL_SCANCODE_W,       2, 1 },
    { SDL_SCANCODE_E,       2, 2 },
    { SDL_SCANCODE_R,       2, 3 },
    { SDL_SCANCODE_T,       2, 4 },

    /* Row 3: 1 2 3 4 5 */
    { SDL_SCANCODE_1,       3, 0 },
    { SDL_SCANCODE_2,       3, 1 },
    { SDL_SCANCODE_3,       3, 2 },
    { SDL_SCANCODE_4,       3, 3 },
    { SDL_SCANCODE_5,       3, 4 },

    /* Row 4: 0 9 8 7 6 */
    { SDL_SCANCODE_0,       4, 0 },
    { SDL_SCANCODE_9,       4, 1 },
    { SDL_SCANCODE_8,       4, 2 },
    { SDL_SCANCODE_7,       4, 3 },
    { SDL_SCANCODE_6,       4, 4 },

    /* Row 5: P O I U Y */
    { SDL_SCANCODE_P,       5, 0 },
    { SDL_SCANCODE_O,       5, 1 },
    { SDL_SCANCODE_I,       5, 2 },
    { SDL_SCANCODE_U,       5, 3 },
    { SDL_SCANCODE_Y,       5, 4 },

    /* Row 6: Enter L K J H */
    { SDL_SCANCODE_RETURN,  6, 0 },
    { SDL_SCANCODE_L,       6, 1 },
    { SDL_SCANCODE_K,       6, 2 },
    { SDL_SCANCODE_J,       6, 3 },
    { SDL_SCANCODE_H,       6, 4 },

    /* Row 7: Space Sym M N B */
    { SDL_SCANCODE_SPACE,   7, 0 },
    { SDL_SCANCODE_LCTRL,   7, 1 },  /* Symbol Shift = Ctrl */
    { SDL_SCANCODE_RCTRL,   7, 1 },
    { SDL_SCANCODE_M,       7, 2 },
    { SDL_SCANCODE_N,       7, 3 },
    { SDL_SCANCODE_B,       7, 4 },

    /* Extra mappings for convenience */
    { SDL_SCANCODE_BACKSPACE, 4, 0 },  /* 0 = DELETE in ZX */
    { SDL_SCANCODE_ESCAPE,    0, 0 },  /* Maps to CAPS SHIFT + SPACE */
};

#define KEYMAP_COUNT (sizeof(keymap) / sizeof(keymap[0]))

void keyboard_key_down(sp_machine_t *m, u32 scancode) {
    /* Special: Escape = CAPS SHIFT + SPACE */
    if (scancode == SDL_SCANCODE_ESCAPE) {
        m->key_matrix[0] &= ~(1 << 0);  /* CAPS SHIFT */
        m->key_matrix[7] &= ~(1 << 0);  /* SPACE */
        return;
    }

    for (int i = 0; i < (int)KEYMAP_COUNT; i++) {
        if (keymap[i].scancode == (SDL_Scancode)scancode) {
            m->key_matrix[keymap[i].row] &= ~(1 << keymap[i].bit);
        }
    }
}

void keyboard_key_up(sp_machine_t *m, u32 scancode) {
    if (scancode == SDL_SCANCODE_ESCAPE) {
        m->key_matrix[0] |= (1 << 0);
        m->key_matrix[7] |= (1 << 0);
        return;
    }

    for (int i = 0; i < (int)KEYMAP_COUNT; i++) {
        if (keymap[i].scancode == (SDL_Scancode)scancode) {
            m->key_matrix[keymap[i].row] |= (1 << keymap[i].bit);
        }
    }
}

void keyboard_reset(sp_machine_t *m) {
    memset(m->key_matrix, 0xFF, sizeof(m->key_matrix));
}
