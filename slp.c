#include <stdint.h>
#include "slp.h"

enum {
    ST_Init,
    ST_Searching,
} state;

static MatchStateResponseBuffer *msrb;
static PlayerSelectionsTransferBuffer *pstb;
static FindMatchTransferBuffer *fmtb;

#define GXLINK_HUD 18
static GOBJ *hud_gobj;
static int hud_canvas;

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

static void CSSSceneDecide(void) {
    // Check how CSS was exited
    // TODO: always advance for now

    // Unranked Mode Logic
    // Load Splash Screen
    // SplashSceneInit();

    // try it why not
    *(u8*)OFST_R13_ONLINE_MODE = ONLINE_MODE_UNRANKED;
    Scene_SetNextMajor(8);
    Scene_ExitMajor();
}

static void TM_Think(GOBJ *_) {
    int down = 0;
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

        OSReport("%u %u!\n", msrb->is_local_player_ready, msrb->is_remote_player_ready);
        if (msrb->is_local_player_ready && msrb->is_remote_player_ready)
            stc_match->request_match_end = 1;
    }
}

static void OnTrainingMode(void) {
    OSReport("Enter training mode\n");

    hud_gobj = GObj_Create(19, 20, 0);
    COBJDesc ***dmgScnMdls = Archive_GetPublicAddress(*stc_ifall_archive, (void *)0x803f94d0);
    COBJDesc *cam_desc = dmgScnMdls[1][0];
    COBJ *hud_cobj = COBJ_LoadDesc(cam_desc);
    GObj_AddObject(hud_gobj, R13_U8(-0x3E55), hud_cobj);
    GOBJ_InitCamera(hud_gobj, CObjThink_Common, 7);
    hud_gobj->cobj_links = 1 << GXLINK_HUD;
    hud_canvas = Text_CreateCanvas(2, hud_gobj, 14, 15, 0, GXLINK_HUD, 81, 19);

    searching_text = Text_CreateText(2, hud_canvas);
    searching_text->kerning = 1;
    searching_text->viewport_scale.X = 0.1f;
    searching_text->viewport_scale.Y = 0.1f;
    Text_AddSubtext(searching_text, 0, 0, "");
    Text_SetPosition(searching_text, 0, -270.f, -170.f);

    pstb = HSD_MemAlloc(sizeof(*pstb));
    msrb = HSD_MemAlloc(sizeof(*msrb));
    fmtb = HSD_MemAlloc(sizeof(*fmtb));

    // we need to call this because m-ex fucks us and overwrites the scene table
    MajorSceneDesc *major_table = Scene_GetMajorSceneDesc();

    for (int j = 0; j < MJRKIND_NULL; ++j) {
        if (major_table[j].major_id == MJRKIND_TRAIN) {
            MajorSceneDesc *major_tm = &major_table[j];
            for (int i = 0; ; ++i) {
                MinorScene *minor = &major_tm->minor_scene_arr[i];
                if (minor->minor_id == -1) break;
                if (minor->minor_kind == MNRKIND_TRAIN)
                    minor->minor_decide = CSSSceneDecide;
            }
            break;
        }
    }

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

void OnSceneChange(void) {
    u32 major = stc_scene_info->major_curr;
    u32 minor = stc_scene_info->minor_curr;
    OSReport("%u %u\n", major, minor);
    if (major == MJRKIND_TRAIN && minor == MNRKIND_MATCH) OnTrainingMode();
}

