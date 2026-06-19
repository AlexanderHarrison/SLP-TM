#include <stdint.h>
#include <stddef.h>
#include "slp.h"
#include "common.c"
#include "menu.c"

enum {
    ST_Init,
    ST_Searching,
} state;

static MatchStateResponseBuffer *msrb;
static PlayerSelectionsTransferBuffer *pstb;
static FindMatchTransferBuffer *fmtb;

static Text *searching_text;
static char *searching_text_variants[] = { "Searching", "Searching.", "Searching..", "Searching..." };
static u32 searching_text_counter = 0;

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

static Rect MenuRect(f32 x, f32 y) {
    return (Rect) {
        x - 8.f, y - 1.5f,
        x + 8.f, y + 1.5f,
    };
}

static void TM_GX(GOBJ *gobj, int pass) {
    HUD_StartGX();

    Rect test = MenuRect(0,0);
    GXColor outline = {230, 230, 230, 255};
    GXColor inside = {90, 90, 90, 130};
    HUD_DrawMenuItem(&test, outline, inside);

    HUD_DrawText("Unranked", &test, 0.5f, outline, false);

    HUD_EndGX();
}

static void TM_Think(GOBJ *_) {
    /*int down = 0;
    for (int i = 0; i < 4; ++i) down |= PadGetMaster(i)->down;

    if (state == ST_Init && (down & HSD_BUTTON_A)) {
        OSReport("setting character\n");
        SlpEXITransferBuffer(pstb, sizeof(*pstb), SlpExiWrite);

        OSReport("finding match\n");
        SlpEXITransferBuffer(fmtb, sizeof(*fmtb), SlpExiWrite);

        state = ST_Searching;
    }
    else if (state == ST_Searching) {
        searching_text_counter++;
        u32 i = (searching_text_counter / 32) % 4;
        Text_SetText(searching_text, 0, searching_text_variants[i]);

        // use msrb to send 
        msrb->connection_state = SlippiCmdGetMatchState;
        SlpEXITransferBuffer(msrb, 1, SlpExiWrite);
        SlpEXITransferBuffer(msrb, sizeof(*msrb), SlpExiRead);

        if (msrb->is_local_player_ready && msrb->is_remote_player_ready)
            stc_match->request_match_end = 1;
    }*/
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

    // SETUP HUD -----------------------------------------------
    
    HUD_Init();
    GObj_AddGXLink(hud_gobj, TM_GX, GXLINK_HUD, 0);

    // INIT -----------------------------------------------
    
    // When I put this in HUD_Init it doesn't work (???????)
    GOBJ *test = GObj_Create(19, 20, 0);
    COBJDesc ***dmgScnMdls = Archive_GetPublicAddress(*stc_ifall_archive, (void *)0x803f94d0);
    COBJDesc *cam_desc = dmgScnMdls[1][0];
    COBJ *test_cobj = COBJ_LoadDesc(cam_desc);
    GObj_AddObject(test, R13_U8(-0x3E55), test_cobj);
    GOBJ_InitCamera(test, CObjThink_Common, 7);
    test->cobj_links = 1 << 18;
    hud_canvas = Text_CreateCanvas(2, test, 14, 15, 0, 18, 81, 19);

    // searching_text = Text_CreateText(2, hud_canvas);
    // searching_text->kerning = 1;
    // searching_text->viewport_scale.X = 0.1f;
    // searching_text->viewport_scale.Y = 0.1f;
    // Text_AddSubtext(searching_text, 0, 0, "AKJSDHAKJSD");
    // Text_SetPosition(searching_text, 0, 0.f, 0.f);

    // SETUP SLIPPI BUFFERS -----------------------------------------------

    pstb = HSD_MemAlloc(sizeof(*pstb));
    msrb = HSD_MemAlloc(sizeof(*msrb));
    fmtb = HSD_MemAlloc(sizeof(*fmtb));

    *pstb = (PlayerSelectionsTransferBuffer) {
        .cmd = SlippiCmdSetMatchSelections,
        .team_id = 0,
        .char_id = CKIND_FOX,
        .char_color = 0,
        .char_opt = 1,
        .stage_id = GRKIND_FD,
        .stage_opt = 3,
        .online_mode = ONLINE_MODE_UNRANKED,
    };

    *fmtb = (FindMatchTransferBuffer) {
        .cmd = SlippiCmdFindOpponent,
        .online_mode = ONLINE_MODE_UNRANKED,
    };

    GOBJ *g = GObj_Create(0, 0, 0);
    GObj_AddProc(g, TM_Think, 20);
}

static void TM_Exit(void *_) {}

static void TM_Decide(void) {
    // Check how CSS was exited
    // TODO: always advance for now

    // try it why not
    *(u8*)OFST_R13_ONLINE_MODE = ONLINE_MODE_UNRANKED;
    Scene_SetNextMajor(8);
    Scene_ExitMajor();
}

#ifdef DEBUG
    static void OnMenu(void) {
        Scene_SetNextMajor(MJRKIND_TRAIN);
        Scene_ExitMajor();
    }
#endif

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

void OnSceneChange(void) {
    u32 major = stc_scene_info->major_curr;
    u32 minor = stc_scene_info->minor_curr;
    if (major == MJRKIND_TRAIN && minor == MNRKIND_MATCH) {
        OverrideSceneDecide(TM_Decide);
        TM_Init();
    }

    #ifdef DEBUG
        if (major == MJRKIND_MNMA) {
            OverrideSceneDecide(OnMenu);
            Scene_ExitMinor();
        }
    #endif
}

