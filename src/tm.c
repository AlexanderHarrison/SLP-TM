#include <stdint.h>
#include <stddef.h>
#include "slp.h"
#include "common.c"
#include "menu.c"

enum {
    ST_Init,
    ST_Searching,
} state;

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

void bp(void);

/*
main.asm:
  on enter CSS:
    run MajorSceneLoad function (callback from Scene_ProcessMajor 801a4418)
    run CSSScenePrep function (callback from Scene_ProcessMinor 801a40b0)
  after connection:
    run CSSSceneDecide
    run SplashSceneInit
    run SplashScenePrep
  after splash scene runs:
    run SplashSceneDecide

  game happens (immediate exit from testing due to breakpoints causing timeout (?))
    run SinglesDetermineWinner
    run VSSceneDecide
    run CheckIfWonLastGame from VSSceneDecide_UpdateWinner 
*/

static Anim queue_indicator_pop = {
    -20, -20,
    3, 0,
    -20, 0
};

static void Click_Exit(MenuItem *item) {
    exit_mode = EXIT_CSS;
    Scene_ExitMinor();
}

static void SlpEXIByteCmd(u8 cmd) {
    *byteb = cmd;
    SlpEXITransferBuffer(byteb, 1, SlpExiWrite);
}

static void Queue_Stop(void) {
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

enum {
    TEST_MENU_QUEUE_RANKED,
    TEST_MENU_QUEUE_UNRANKED,
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
        .name = "Exit",
        .Click = Click_Exit,
    },
};
static Menu test_menu = { countof(test_menu_items), test_menu_items };

static s32 menu_selection_cur;
static s32 menu_selection_prev;

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

static const f32 selection_change_t = 4.f;
static const f32 selection_change_fast_t = 2.f;
static const f32 selection_change_r_t = 1.f/selection_change_t;
static const f32 selection_change_fast_r_t = 1.f/selection_change_fast_t;

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

static Anim menu_scroll = { 0 }; // adjusted at runtime

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

static void Menu_GX(void) {
    f32 menu_x = 10;
    f32 menu_y = menu_pop.cur;
    s32 i = test_menu.item_count - 1;
    f32 t = menu_scroll.cur + menu_pop_scroll.cur - ((f32)i - 4.f);
    while (i >= 0) {
        MenuItem *item = &test_menu.items[i];
        
        static const f32 y_scale = 3.5f;
        static const f32 selected_x_increase = -4.f;
        static const f32 selected_y_increase = 1.5f;

        // f32 item_x = t*t * 0.15f;
        f32 item_x = t*t * 0.15f - t*t*t/120.f;
        f32 item_y = y_scale*t;
        Rect menu_rect = MenuRect(menu_x + item_x, menu_y + item_y);

        if (i == menu_selection_cur) {
            f32 cur = menu_selected_item_cur.cur;
            f32 dy = cur * selected_y_increase;
            f32 dx = cur * selected_x_increase;
            menu_rect.y1 += dy;
            menu_rect.x0 += dx;
            menu_rect.x1 += dx;
            t += dy / y_scale;
        }
        if (i == menu_selection_prev) {
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

static void TM_GX(GOBJ *gobj, int pass) {
    HUD_StartGX();

    Menu_GX();
    Queue_GX();

    HUD_EndGX();
}

static void TM_Think(GOBJ *_) {
    int combined_pad_down = 0; 
    int combined_pad_held = 0; 
    for (int i = 0; i < 4; ++i) {
        HSD_Pad *pad = PadGetMaster(i);
        combined_pad_down |= pad->down;
        combined_pad_held |= pad->held;
    }

    // PAUSE ------------------------------------------------------------------------
    
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

    // MENU ------------------------------------------------------------------------
    
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
        menu_scroll.speed = fast ? selection_change_fast_r_t : selection_change_r_t;

        bool repeat_input = false;
        if (repeat_timer >= repeat_time) {
            repeat_timer = repeat_continue_time;
            repeat_input = true;
        }

        int pad_scroll = repeat_input ? combined_pad_held : combined_pad_down;

        if ((pad_scroll & PAD_BUTTON_UP) && menu_selection_cur > 0) {
            selection_changed = true;
            menu_scroll.target -= 1.f;
            menu_selection_prev = menu_selection_cur;
            menu_selection_cur -= 1;
        }

        if ((pad_scroll & PAD_BUTTON_DOWN) && menu_selection_cur+1 < test_menu.item_count) {
            selection_changed = true;
            menu_scroll.target += 1.f;
            menu_selection_prev = menu_selection_cur;
            menu_selection_cur += 1;
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
        
        MenuItem *selected_item = &test_menu.items[menu_selection_cur];
        if (combined_pad_down & PAD_BUTTON_A) {
            // static int i = 340;
            // 105 148 157 183 230 243 249 287 297 347-9 398 418 451 459 490
            // OSReport("%i\n", i);
            // SFX_Play(i++);
            selected_item->Click(selected_item);
        }
    }
    
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
    Anim_Update(&menu_scroll);
    Anim_Update(&menu_selected_item_prev);
    Anim_Update(&menu_selected_item_cur);
    Anim_Update(&queue_indicator_pop);
}

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
    
    // void (*hud)(int mode) = (void*)0x802f665c;
    // hud(4);
    // for (int i = 0; i < 4; ++i)
    // Match_CreateHUD(0);
    // Match_ShowHUD();
    // Match_ShowPercents();
    // Match_ShowTimer();
    // *stc_hud_is_hidden = false;
    // Match_CreateHUD(0);

    HUD_Init();
    GObj_AddGXLink(hud_gobj, TM_GX, GXLINK_HUD, 20); // TODO: causing some issue with showing percents hud

    // When I put this in HUD_Init it doesn't work (???????)
    GOBJ *test = GObj_Create(19, 20, 0);
    COBJDesc ***dmgScnMdls = Archive_GetPublicAddress(*stc_ifall_archive, (void *)0x803f94d0);
    COBJDesc *cam_desc = dmgScnMdls[1][0];
    COBJ *test_cobj = COBJ_LoadDesc(cam_desc);
    GObj_AddObject(test, R13_U8(-0x3E55), test_cobj);
    GOBJ_InitCamera(test, CObjThink_Common, 7);
    test->cobj_links = 1 << 18;
    hud_canvas = Text_CreateCanvas(2, test, 14, 15, 0, 18, 81, 19);

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

    // #ifdef DEBUG
    //     if (major == MJRKIND_MNMA) {
    //         OverrideSceneDecide(OnMenu);
    //         Scene_ExitMinor();
    //     }
    // #endif
}
