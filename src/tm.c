#include <stdint.h>
#include <stddef.h>
#include "slp.h"
#include "common.c"

void bp(void);

// static data doesn't work for some reason, need to allocate these
static MatchStateResponseBuffer *msrb;
static PlayerSelectionsTransferBuffer *pstb;
static FindMatchTransferBuffer *fmtb;
static u8 *byteb;

static bool paused = false;
static u32 queue_timer = 0;

enum {
    EXIT_CSS,
    EXIT_SLIPPI,
};
static u8 exit_mode;

// ANIMATIONS ----------------------------------------------------------------

static const f32 selection_change_t = 4.f;
static const f32 selection_change_fast_t = 2.f;
static const f32 selection_change_r_t = 1.f/selection_change_t;
static const f32 selection_change_fast_r_t = 1.f/selection_change_fast_t;

static Anim queue_indicator_pop = {
    -20, -20,
    3, 0,
    -20, 0
};

static Anim menu_pop = {
    -50, -50,
    3.f, 0,
    -50, -22
};

static Anim menu_pop_scroll = {
    -20, -20,
    2.1f, 0,
    -20, 0
};

static Anim menu_selected_item_prev = {
    0, 0,
    selection_change_r_t, 0,
    0, 1,
};
static Anim menu_selected_item_cur = {
    1, 1,
    selection_change_r_t, 0,
    0, 1,
};

// MENU PROTOTYPES ----------------------------------------------------------------

static void Click_Exit(MenuItem *item);
static void Click_QueueRanked(MenuItem *item);
static void Click_QueueUnranked(MenuItem *item);
static void Click_QueueDirect(MenuItem *item);
static void Click_QueueTeams(MenuItem *item);
static void Click_QueueParty(MenuItem *item);

static void Menu_Think(Menu *menu);
static void Menu_GX(Menu *menu);
static void TextInput_Think(Menu *menu);
static void TextInput_GX(Menu *menu);

// MENUS ----------------------------------------------------------------

enum {
    TEST_MENU_QUEUE_RANKED,
    TEST_MENU_QUEUE_UNRANKED,
    TEST_MENU_QUEUE_DIRECT,
    TEST_MENU_QUEUE_TEAMS,
    TEST_MENU_QUEUE_PARTY,
    TEST_MENU_EXIT,
};

static MenuItem test_menu_items[] = {
    {
        .name = "Queue Ranked",
        .Click = Click_QueueRanked,
    },
    {
        .name = "Queue Unranked",
        .Click = Click_QueueUnranked,
    },
    {
        .name = "Queue Direct",
        .Click = Click_QueueDirect,
    },
    {
        .name = "Queue Teams",
        .Click = Click_QueueTeams,
    },
    {
        .name = "Queue Party",
        .Click = Click_QueueParty,
    },
    {
        .name = "Exit",
        .Click = Click_Exit,
    },
};
static Menu test_menu = {
    .item_count = countof(test_menu_items),
    .items = test_menu_items,
    .Think = Menu_Think,
    .GX = Menu_GX,
};

// todo can I remove most of this?
static char connect_code_buf[18];
static u32 connect_code_buf_len;
static MenuItem *connect_code_text_input_menu_item;
static u8 connect_code_text_input_online_mode;

static Menu connect_code_text_input_menu = {
    .Think = TextInput_Think,
    .GX = TextInput_GX,
};

static Menu *menu_stack[32] = { &test_menu };
static int menu_depth;

// MENU CALLBACKS ----------------------------------------------------------------

static void Click_Exit(MenuItem *item) {
    exit_mode = EXIT_CSS;
    Scene_ExitMinor();
}

static void SlpEXIByteCmd(u8 cmd) {
    *byteb = cmd;
    SlpEXITransferBuffer(byteb, 1, SlpExiWrite);
}

static void Queue_Stop(void) {
    MenuItem_Off(&test_menu_items[TEST_MENU_QUEUE_RANKED]);
    MenuItem_Off(&test_menu_items[TEST_MENU_QUEUE_UNRANKED]);
    MenuItem_Off(&test_menu_items[TEST_MENU_QUEUE_DIRECT]);
    MenuItem_Off(&test_menu_items[TEST_MENU_QUEUE_TEAMS]);
    MenuItem_Off(&test_menu_items[TEST_MENU_QUEUE_PARTY]);

    if (queue_timer) {
        queue_timer = 0;
        SlpEXIByteCmd(SlippiCmdCleanupConnections);
    }
}

static void Click_Queue(MenuItem *item, u8 online_mode) {
    Queue_Stop();

    if (MenuItem_Toggle(item)) {
        Anim_Start(&queue_indicator_pop);
        queue_timer = 1;
        pstb->online_mode = online_mode;
        fmtb->online_mode = online_mode;
        *(u8*)OFST_R13_ONLINE_MODE = online_mode;

        SlpEXITransferBuffer(pstb, sizeof(*pstb), SlpExiWrite);
        SlpEXITransferBuffer(fmtb, sizeof(*fmtb), SlpExiWrite);
    } else {
        Anim_StartRev(&queue_indicator_pop);
    }
}

static void Click_QueueRanked(MenuItem *item) { Click_Queue(item, ONLINE_MODE_RANKED); }
static void Click_QueueUnranked(MenuItem *item) { Click_Queue(item, ONLINE_MODE_UNRANKED); }
static void Click_QueueDirect(MenuItem *item) {
    menu_stack[++menu_depth] = &connect_code_text_input_menu; // potential sync issues with midframe menu change?
    connect_code_text_input_menu_item = item;
    connect_code_text_input_online_mode = ONLINE_MODE_DIRECT;
}
static void Click_QueueTeams(MenuItem *item) {
    menu_stack[++menu_depth] = &connect_code_text_input_menu; // potential sync issues with midframe menu change?
    connect_code_text_input_menu_item = item;
    connect_code_text_input_online_mode = ONLINE_MODE_TEAMS;
}
static void Click_QueueParty(MenuItem *item) {
    menu_stack[++menu_depth] = &connect_code_text_input_menu; // potential sync issues with midframe menu change?
    connect_code_text_input_menu_item = item;
    connect_code_text_input_online_mode = ONLINE_MODE_PARTY;
}

// GX ----------------------------------------------------------------

typedef struct QueueIndicatorInfo {
    const char *text;
    GXColor outline_color;
    GXColor inside_color;
} QueueIndicatorInfo;

static const GXColor queue_indicator_text_color = menu_colour_default_text;
static const GXColor queue_indicator_timer_text_color = menu_colour_default_subtext;
static const QueueIndicatorInfo queue_indicator_info[] = {
    {
        "Ranked...",
        menu_colour_default_outline,
        menu_colour_default_inside,
    },
    {
        "Unranked...",
        {255, 150, 30, 255},
        {70, 60, 50, 200},
    },
    {
        "Direct...",
        menu_colour_default_outline,
        menu_colour_default_inside,
    },
    {
        "Teams...",
        menu_colour_default_outline,
        menu_colour_default_inside,
    },
    {
        "Party...",
        menu_colour_default_outline,
        menu_colour_default_inside,
    },
};

static void Queue_GX(void) {
    const QueueIndicatorInfo *info = &queue_indicator_info[fmtb->online_mode];

    Rect menu_rect = {
        -28, 18,
        -13, 21,
    };
    
    menu_rect.x0 += queue_indicator_pop.cur;
    menu_rect.x1 += queue_indicator_pop.cur;
    
    HUD_DrawMenuItem(&menu_rect, info->outline_color, info->inside_color);
    HUD_DrawText(info->text, &menu_rect, 0.5f, queue_indicator_text_color, false);
    
    char timer_buf[16];
    char *s = &timer_buf[countof(timer_buf)];
    *--s = 0;

    u32 timer = queue_timer / 60;
    if (timer < 60) {
        *--s = 's';
        menu_rect.x0 = menu_rect.x1 - 2.5f;
    } else {
        timer /= 60;
        *--s = 'm';
        menu_rect.x0 = menu_rect.x1 - 3;
    }

    do {
        *--s = '0' + timer % 10;
        timer /= 10;
        menu_rect.x0 -= 0.6f;
    } while (timer);
    
    HUD_DrawText(s, &menu_rect, 0.4f, queue_indicator_timer_text_color, false);
}

static void Menu_GX(Menu *menu) {
    f32 menu_x = 10;
    f32 menu_y = menu_pop.cur;
    s32 i = menu->item_count - 1;
    f32 t = menu->scroll.cur + menu_pop_scroll.cur - ((f32)i - 4.f);
    while (i >= 0) {
        MenuItem *item = &menu->items[i];
        
        static const f32 y_scale = 3.5f;
        static const f32 selected_x_increase = -4.f;
        static const f32 selected_y_increase = 1.5f;

        // f32 item_x = t*t * 0.15f;
        // f32 item_x = t*t * 0.15f - t*t*t/120.f;
        f32 item_x = 7.f;
        f32 item_y = y_scale*t;
        Rect menu_rect = MenuRect(menu_x + item_x, menu_y + item_y);

        if (i == menu->selected_idx_cur) {
            f32 cur = menu_selected_item_cur.cur;
            f32 dy = cur * selected_y_increase;
            f32 dx = cur * selected_x_increase;
            menu_rect.y1 += dy;
            menu_rect.x0 += dx;
            menu_rect.x1 += dx;
            t += dy / y_scale;
        }
        if (i == menu->selected_idx_prev) {
            f32 cur = menu_selected_item_prev.cur;
            f32 dy = cur * selected_y_increase;
            f32 dx = cur * selected_x_increase;
            menu_rect.y1 += dy;
            menu_rect.x0 += dx;
            menu_rect.x1 += dx;
            t += dy / y_scale;
        }
        
        GXColor inside = item->inside_color;
        GXColor outline = item->outline_color;
        GXColor text = item->text_color;
        u32 zero = 0;
        if (memcmp(&inside, &zero, 4) == 0) inside = menu_colour_default_inside;
        if (memcmp(&outline, &zero, 4) == 0) outline = menu_colour_default_outline;
        if (memcmp(&text, &zero, 4) == 0) text = menu_colour_default_text;

        HUD_DrawMenuItem(&menu_rect, outline, inside);
        HUD_DrawText(item->name, &menu_rect, 0.5f, text, false);

        i -= 1; t += 1.f;
    }
}

static void TextInput_DrawSide(f32 x, f32 y, char chars[15], char selected_char) {
    GXColor outline = {255, 255, 255, 255};
    GXColor outline_selected = {255, 100, 100, 255};
    GXColor inside = {0, 0, 0, 255};

    static const f32 square_size = 3;
    static const f32 square_pad = 0.2f;

    for (int y_i = 0; y_i < 3; ++y_i) {
        for (int x_i = 0; x_i < 5; ++x_i) {
            Rect r = { x, y, x + square_size, y + square_size };
            char c = chars[y_i*5 + x_i];

            GXColor o = c == selected_char ? outline_selected : outline;
            HUD_DrawMenuItem(&r, o, inside);

            char text[2] = {c, 0};
            HUD_DrawText(text, &r, 0.5f, o, false);
            x += square_size + square_pad;
        }

        y -= square_size + square_pad;
        x -= (square_size + square_pad) * 5.f;
    }
}

static void TextInput_GX(Menu *_) {
    HSD_Pad *pad = PadGetMaster(1); // TODO: ply
    char lstick_char = TextInput_LStickChar(pad->stickX, pad->stickY);
    char cstick_char = TextInput_CStickChar(pad->substickX, pad->substickY);
    TextInput_DrawSide(-10, 8, "QWERTASDFGZXCVB", lstick_char);
    TextInput_DrawSide(8, 8, "YUIOPHJKL.NM012", cstick_char);

    Rect r = { -10, 10, 10, 15 };
    HUD_DrawText(connect_code_buf, &r, 0.5f, menu_colour_default_text, false);
}

static void TM_GX(GOBJ *gobj, int pass) {
    if (pass == 1) {
        HUD_StartGX();
        
        Menu *menu = menu_stack[menu_depth];
        menu->GX(menu);

        Queue_GX();
    
        HUD_EndGX();
    }
}

// THINK ----------------------------------------------------------------------------------------

static void TextInput_Think(Menu *menu) {
    HSD_Pad *pad = PadGetMaster(1); // TODO: ply
    char lstick_char = TextInput_LStickChar(pad->stickX, pad->stickY);
    char cstick_char = TextInput_CStickChar(pad->substickX, pad->substickY);

    // TODO: count light press (but don't double count lol)

    if (connect_code_buf_len+1 < countof(connect_code_buf)) {
        if (pad->down & PAD_TRIGGER_L) connect_code_buf[connect_code_buf_len++] = lstick_char;
        else if (pad->down & PAD_TRIGGER_R) connect_code_buf[connect_code_buf_len++] = cstick_char;
    }

    if (connect_code_buf_len && (pad->down & PAD_TRIGGER_Z))
        connect_code_buf[--connect_code_buf_len] = 0;
    
    if (pad->down & PAD_BUTTON_START) {
        memcpy(fmtb->opp_connect_code, connect_code_buf, sizeof(connect_code_buf));
        Click_Queue(connect_code_text_input_menu_item, connect_code_text_input_online_mode);
        menu_depth--;
    }
    else if (pad->down & PAD_BUTTON_B) {
        menu_depth--;
    }
}

static void Menu_Think(Menu *menu) {
    int combined_pad_down = 0; 
    int combined_pad_held = 0; 
    for (int i = 0; i < 4; ++i) {
        HSD_Pad *pad = PadGetMaster(i);
        combined_pad_down |= pad->down;
        combined_pad_held |= pad->held;
    }

    if (combined_pad_down & PAD_BUTTON_START) {
        if (paused) {
            paused = false;
            Match_ShowHUD();
            Match_UnfreezeGame(1);
            Anim_StartRev(&menu_pop);
            Anim_StartRev(&menu_pop_scroll);
        } else {
            paused = true;
            Match_HideHUD();
            Match_FreezeGame(1);
            SFX_PlayCommon(5);
            Anim_Start(&menu_pop);
            Anim_Start(&menu_pop_scroll);
        }
    }
    
    if (paused) {
        bool selection_changed = false;
    
        static int repeat_timer = 0;
        static const int repeat_time_slow = 30;
        static const int repeat_time_fast = 2;
        static const int repeat_continue_time_slow = 24;
        static const int repeat_continue_time_fast = 0;
    
        if ((combined_pad_held & (PAD_BUTTON_UP | PAD_BUTTON_DOWN)) == 0)
            repeat_timer = 0;
        else
            repeat_timer++;
        
        bool fast = combined_pad_held & PAD_TRIGGER_R;
        int repeat_time = fast ? repeat_time_fast : repeat_time_slow;
        int repeat_continue_time = fast ? repeat_continue_time_fast : repeat_continue_time_slow;
        menu->scroll.speed = fast ? selection_change_fast_r_t : selection_change_r_t;
    
        bool repeat_input = false;
        if (repeat_timer >= repeat_time) {
            repeat_timer = repeat_continue_time;
            repeat_input = true;
        }
    
        int pad_scroll = repeat_input ? combined_pad_held : combined_pad_down;
    
        if ((pad_scroll & PAD_BUTTON_UP) && menu->selected_idx_cur > 0) {
            selection_changed = true;
            menu->scroll.target -= 1.f;
            menu->selected_idx_prev = menu->selected_idx_cur;
            menu->selected_idx_cur -= 1;
        }
    
        if ((pad_scroll & PAD_BUTTON_DOWN) && menu->selected_idx_cur+1 < test_menu.item_count) {
            selection_changed = true;
            menu->scroll.target += 1.f;
            menu->selected_idx_prev = menu->selected_idx_cur;
            menu->selected_idx_cur += 1;
        }
    
        // static u32 sfx_i= 0;
        // static int sfx[] = {
        //     183,287,490
        // };
        // menu change? 105 148
        // 230 243 249 ok if remove echo
        /*if (combined_pad_down & PAD_TRIGGER_Z) {
            sfx_i++;
            if (sfx_i == countof(sfx)) sfx_i = 0;
            OSReport("%i\n", sfx[sfx_i]);
        }*/
    
        if (selection_changed) {
            SFX_PlayRaw(287, 100, 128, 2, 0);
            // SFX_Play(sfx[sfx_i]);
            menu_selected_item_prev = menu_selected_item_cur;
            Anim_StartRev(&menu_selected_item_prev);
    
            menu_selected_item_cur.cur = menu_selected_item_cur.start;
            Anim_Start(&menu_selected_item_cur);
        }
        
        MenuItem *selected_item = &menu->items[menu->selected_idx_cur];
        if (combined_pad_down & PAD_BUTTON_A) {
            // static int i = 340;
            // 105 148 157 183 230 243 249 287 297 347-9 398 418 451 459 490
            // OSReport("%i\n", i);
            // SFX_Play(i++);
            selected_item->Click(selected_item);
        }
    }
}

static void TM_Think(GOBJ *_) {
    Menu *menu = menu_stack[menu_depth];
    menu->Think(menu);

    // QUEUE ------------------------------------------------------------------------
    
    if (queue_timer) {
        queue_timer++;

        SlpEXIByteCmd(SlippiCmdGetMatchState);
        SlpEXITransferBuffer(msrb, sizeof(*msrb), SlpExiRead);

        if (msrb->is_local_player_ready && msrb->is_remote_player_ready) {
            exit_mode = EXIT_SLIPPI;
            Scene_ExitMinor();
        }
    }

    // ANIMATIONS ------------------------------------------------------------------------
    
    Anim_Update(&menu_pop);
    Anim_Update(&menu_pop_scroll);
    Anim_Update(&test_menu.scroll);
    Anim_Update(&menu_selected_item_prev);
    Anim_Update(&menu_selected_item_cur);
    Anim_Update(&queue_indicator_pop);
}

// INIT ----------------------------------------------------------------------------------------

static void TM_Init(void) {
    // REMOVE VANILLA STUFF -----------------------------------------------

    // remove basic TM hud
    for (GOBJ *gobj = (*stc_gobj_lookup)[MATCHPLINK_2]; gobj; gobj = gobj->next) {
        OSReport("removing %p\n", gobj);
        GObj_Destroy(gobj);
    }

    GOBJ *vanilla_hud_gobj = *(GOBJ**)0x804738b8;
    GObj_Destroy(vanilla_hud_gobj);

    // remove default update
    stc_hsd_update->checkPause = NULL;
    stc_hsd_update->checkAdvance = NULL;
    stc_hsd_update->onFrame = NULL;

    // SETUP HUD -----------------------------------------------

    HUD_Init();
    GObj_AddGXLink(hud_gobj, TM_GX, GXLINK_HUD, 20);

    // SETUP SLIPPI BUFFERS -----------------------------------------------

    pstb = HSD_MemAlloc(sizeof(*pstb));
    msrb = HSD_MemAlloc(sizeof(*msrb));
    fmtb = HSD_MemAlloc(sizeof(*fmtb));
    byteb = HSD_MemAlloc(8);

    int ply = 0; // TODO: not right logic
    Playerblock *pb = Fighter_GetPlayerblock(ply);

    *pstb = (PlayerSelectionsTransferBuffer) {
        .cmd = SlippiCmdSetMatchSelections,
        .team_id = 0,
        .char_id = pb->c_kind,
        .char_color = pb->costume,
        .char_opt = 1, // idk what this does, slippi asm always sets to one
        .stage_opt = 3, // random
        // .online_mode = set later
    };

    *fmtb = (FindMatchTransferBuffer) {
        .cmd = SlippiCmdFindOpponent,
        // .online_mode = set later
    };

    GOBJ *g = GObj_Create(0, 0, 0);
    GObj_AddProc(g, TM_Think, 20);
}

static void TM_Decide(void) {
    switch (exit_mode) {
        case EXIT_CSS: {
            Queue_Stop();
            Scene_SetNextMinor(0);
        } break;

        case EXIT_SLIPPI: {
            // set css character
            int ply = 0; // TODO: not right logic
            Playerblock *pb = Fighter_GetPlayerblock(ply);
            stc_memcard->EventBackup.c_kind = pb->c_kind;
            stc_memcard->EventBackup.costume = pb->costume;
            // TODO: set port?

            Scene_SetNextMajor(8);
            Scene_ExitMajor();
        } break;
    }
}

static void OverrideSceneDecide(void (*decide)(void)) {
    u8 major_id = Scene_GetCurrentMajor();
    u8 minor_id = Scene_GetCurrentMinor();

    MajorSceneDesc *major_table = Scene_GetMajorSceneDesc();
    for (int j = 0; j < MJRKIND_NULL; ++j) {
        MajorSceneDesc *major = &major_table[j];
        if (major->major_id == major_id) {
            for (int i = 0; ; ++i) {
                MinorScene *minor = &major->minor_scene_arr[i];
                if (minor->minor_id == -1)
                    break;
                if (minor->minor_id == minor_id)
                    minor->minor_decide = decide;
            }
            break;
        }
    }
}

#ifdef DEBUG
    static void OnMenu(void) {
        Scene_SetNextMajor(MJRKIND_TRAIN);
        Scene_ExitMajor();
    }
#endif

void OnSceneChange(void) {
    Scene_GetMinorSceneDesc()[MNRKIND_TRAIN].cb_Think = NULL;

    u32 major = stc_scene_info->major_curr;
    u32 minor = stc_scene_info->minor_curr;
    if (major == MJRKIND_TRAIN && minor == MNRKIND_MATCH) {
        OverrideSceneDecide(TM_Decide);
        TM_Init();
    }

    #ifdef DEBUG
        stc_memcard->EventBackup.c_kind = CKIND_PEACH;
        stc_memcard->EventBackup.costume = 3;
        if (major == MJRKIND_MNMA) {
            OverrideSceneDecide(OnMenu);
            Scene_ExitMinor();
        }
    #endif
}
