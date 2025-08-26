// saturn_post.c
#include <yaul.h>
#include <stdbool.h>

// --- Sample beep for sound test ---
static const uint8_t beep_sample[32] = {
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- 16x16 white sprite (RGB555 16-bit) ---
static const uint16_t sprite_bitmap[16*16] = {
    [0 ... 255] = 0x7FFF  // White pixel
};

// --- Function prototypes ---
static void memory_test(void);
static void color_test(void);
static void monitor_controller(void);
static void sound_test(void);
static void sprite_test(void);
static bool vram_test(void);
static void cpu_register_dump(void);
static void run_menu(void);

// --- Memory Test ---
static void memory_test(void) {
    dbgio_printf("Running Memory Test...\n");
    dbgio_flush();

    volatile uint32_t *ram = (volatile uint32_t *)0x20000000; // Example RAM start
    const uint32_t test_len = 0x1000; // Test 4KB for speed

    // Write pattern
    for (uint32_t i = 0; i < test_len; i++) {
        ram[i] = i ^ 0xAAAAAAAA;
    }

    // Verify
    for (uint32_t i = 0; i < test_len; i++) {
        if (ram[i] != (i ^ 0xAAAAAAAA)) {
            dbgio_printf("Memory test FAIL at %08x\n", &ram[i]);
            dbgio_flush();
            return;
        }
    }

    dbgio_printf("Memory test PASS\n");
    dbgio_flush();
    delay(120);
}

// --- Color Test ---
static void color_test(void) {
    dbgio_printf("Running Color Test...\n");
    dbgio_flush();

    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x0), RGB1555(31, 0, 0)); // Red BG
    vdp2_tvmd_display_set();

    delay(60);

    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x0), RGB1555(0, 31, 0)); // Green BG
    delay(60);

    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x0), RGB1555(0, 0, 31)); // Blue BG
    delay(60);

    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x0), RGB1555(31, 31, 31)); // White BG
    delay(60);

    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x0), RGB1555(0, 0, 0)); // Back to black
    dbgio_printf("Color Test complete.\n");
    dbgio_flush();
}

// --- Controller Test ---
static void monitor_controller(void) {
    dbgio_printf("Running Controller Test...\n");
    dbgio_flush();

    pad_port_connect(0);
    pad_state_t pad;

    while (true) {
        pad_port_status_get(0, &pad);

        dbgio_clear();
        dbgio_printf("Controller Input:\n");
        dbgio_printf("Buttons: 0x%04x\n", pad.buttons);
        dbgio_printf("Analog X: %d\n", pad.analog_x);
        dbgio_printf("Analog Y: %d\n", pad.analog_y);
        dbgio_printf("\nPress Start to return\n");
        dbgio_flush();

        if (pad.buttons & PAD_BUTTON_START) {
            delay(10);
            break;
        }

        vdp_sync();
    }
}

// --- Sound Test ---
static void sound_test(void) {
    dbgio_printf("Running Sound Test...\n");
    dbgio_flush();

    scsp_init();

    volatile uint8_t* scsp_ram = (volatile uint8_t*)0x25A00000;

    // Copy sample to SCSP RAM
    for (int i = 0; i < sizeof(beep_sample); i++) {
        scsp_ram[i] = beep_sample[i];
    }

    scsp_ch_t ch = {
        .sd_adr = 0,
        .loop_adr = 0,
        .lps = 0,
        .le = sizeof(beep_sample) - 1,
        .pcm_format = SCSP_PCM_8BIT_UNSIGNED,
        .loop_flag = 0,
        .volume_left = 0x3F,
        .volume_right = 0x3F,
        .pitch = 0x1000,
        .keyon_flag = 0,
    };

    scsp_ch_init(0, &ch);
    scsp_keyon(0);

    delay(60); // Play sound ~1 second

    scsp_keyoff(0);

    dbgio_printf("Sound Test Complete.\n");
    dbgio_flush();
}

// --- Sprite Test ---
static void sprite_test(void) {
    dbgio_printf("Running Sprite Test...\n");
    dbgio_flush();

    vdp1_init();

    volatile uint16_t *vram = (volatile uint16_t *)0x25C00000;
    for (int i = 0; i < 16*16; i++) {
        vram[i] = sprite_bitmap[i];
    }

    vdp1_cmdt_bitmap_t bitmap_cmd = {
        .cmdt = {
            .cmd = VDP1_CMD_BITMAP,
            .cmdt_link = 0xFFFF,
            .color = 0,
            .mode = VDP1_CMD_MODE_NORMAL,
            .bitmap = {
                .format = VDP1_BITMAP_FORMAT_16,
                .clut_mode = VDP1_CLUT_MODE_NONE,
                .width = 16,
                .height = 16,
                .addr = 0,
            },
            .pat = {
                .x = 100,
                .y = 100,
            },
        },
    };

    vdp1_cmdt_list_put((vdp1_cmdt_t*)&bitmap_cmd.cmdt);

    vdp1_sync();

    dbgio_printf("Sprite Test complete.\nPress Start to return.\n");
    dbgio_flush();

    pad_port_connect(0);
    pad_state_t pad;

    while (true) {
        pad_port_status_get(0, &pad);
        if (pad.buttons & PAD_BUTTON_START) {
            delay(10);
            break;
        }
        vdp_sync();
    }
}

// --- VRAM Test ---
static bool vram_test(void) {
    dbgio_printf("Running VRAM Test...\n");
    dbgio_flush();

    volatile uint16_t *vram = (volatile uint16_t*)0x25C00000;
    const uint32_t size_words = 0x20000; // 128KB VRAM in words

    for (uint32_t i = 0; i < size_words; i++) {
        vram[i] = (uint16_t)(i ^ 0xAAAA);
    }

    for (uint32_t i = 0; i < size_words; i++) {
        if (vram[i] != (uint16_t)(i ^ 0xAAAA)) {
            dbgio_printf("VRAM test FAIL at %08x\n", &vram[i]);
            dbgio_flush();
            delay(120);
            return false;
        }
    }

    dbgio_printf("VRAM test PASS\n");
    dbgio_flush();
    delay(120);
    return true;
}

// --- CPU Register Dump ---
static void cpu_register_dump(void) {
    uint32_t r0, r1, r2, r3;

    __asm__ volatile ("mov %0, r0" : "=r"(r0));
    __asm__ volatile ("mov %0, r1" : "=r"(r1));
    __asm__ volatile ("mov %0, r2" : "=r"(r2));
    __asm__ volatile ("mov %0, r3" : "=r"(r3));

    dbgio_printf("CPU Registers:\n");
    dbgio_printf("r0: %08x\n", r0);
    dbgio_printf("r1: %08x\n", r1);
    dbgio_printf("r2: %08x\n", r2);
    dbgio_printf("r3: %08x\n", r3);
    dbgio_flush();

    dbgio_printf("Press Start to return.\n");
    dbgio_flush();

    pad_port_connect(0);
    pad_state_t pad;

    while (true) {
        pad_port_status_get(0, &pad);
        if (pad.buttons & PAD_BUTTON_START) {
            delay(10);
            break;
        }
        vdp_sync();
    }
}

// --- Menu ---
typedef enum {
    MENU_MEMORY_TEST,
    MENU_COLOR_TEST,
    MENU_CONTROLLER_TEST,
    MENU_SOUND_TEST,
    MENU_SPRITE_TEST,
    MENU_VRAM_TEST,
    MENU_CPU_REG_DUMP,
    MENU_COUNT
} menu_option_t;

static void run_menu(void) {
    pad_port_connect(0);

    int selected = 0;
    pad_state_t pad;

    while (true) {
        pad_port_status_get(0, &pad);

        if (pad.buttons & PAD_BUTTON_UP) {
            selected = (selected - 1 + MENU_COUNT) % MENU_COUNT;
            delay(10);
        } else if (pad.buttons & PAD_BUTTON_DOWN) {
            selected = (selected + 1) % MENU_COUNT;
            delay(10);
        } else if (pad.buttons & (PAD_BUTTON_A | PAD_BUTTON_START)) {
            dbgio_clear();
            dbgio_printf("Running %s...\n",
                selected == MENU_MEMORY_TEST ? "Memory Test" :
                selected == MENU_COLOR_TEST ? "Color Test" :
                selected == MENU_CONTROLLER_TEST ? "Controller Test" :
                selected == MENU_SOUND_TEST ? "Sound Test" :
                selected == MENU_SPRITE_TEST ? "Sprite Test" :
                selected == MENU_VRAM_TEST ? "VRAM Test" :
                selected == MENU_CPU_REG_DUMP ? "CPU Register Dump" : "Unknown");

            dbgio_flush();

            switch (selected) {
                case MENU_MEMORY_TEST: memory_test(); break;
                case MENU_COLOR_TEST: color_test(); break;
                case MENU_CONTROLLER_TEST: monitor_controller(); break;
                case MENU_SOUND_TEST: sound_test(); break;
                case MENU_SPRITE_TEST: sprite_test(); break;
                case MENU_VRAM_TEST: vram_test(); break;
                case MENU_CPU_REG_DUMP: cpu_register_dump(); break;
            }

            dbgio_clear();
            dbgio_printf("Press Start or A to return to menu\n");
            dbgio_flush();

            while (true) {
                pad_port_status_get(0, &pad);
                if (pad.buttons & (PAD_BUTTON_START | PAD_BUTTON_A)) {
                    delay(10);
                    break;
                }
                vdp_sync();
            }
        }

        dbgio_clear();
        dbgio_printf("Saturn POST Menu:\n");
        for (int i = 0; i < MENU_COUNT; i++) {
            if (i == selected) {
                dbgio_printf("> ");
            } else {
                dbgio_printf("  ");
            }

            dbgio_printf(
                i == MENU_MEMORY_TEST ? "Memory Test\n" :
                i == MENU_COLOR_TEST ? "Color Test\n" :
                i == MENU_CONTROLLER_TEST ? "Controller Test\n" :
                i == MENU_SOUND_TEST ? "Sound Test\n" :
                i == MENU_SPRITE_TEST ? "Sprite Test\n" :
                i == MENU_VRAM_TEST ? "VRAM Test\n" :
                i == MENU_CPU_REG_DUMP ? "CPU Register Dump\n" :
                "Unknown\n"
            );
        }
        dbgio_flush();
        vdp_sync();
    }
}

// --- Main Entry Point ---
int main(void) {
    dbgio_init(DBGIO_DEV_VDP2);

    vdp2_init();

    dbgio_clear();
    dbgio_printf("Saturn POST Diagnostic\n");
    dbgio_printf("Use D-Pad to navigate, A/Start to select\n");
    dbgio_flush();

    run_menu();

    return 0;
}
