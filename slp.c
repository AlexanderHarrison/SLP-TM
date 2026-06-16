#include <stdint.h>
#include "slp.h"

enum {
    ST_Init,
    ST_Searching,
} state;

MatchStateResponseBuffer *msrb;
PlayerSelectionsTransferBuffer *pstb;
FindMatchTransferBuffer *fmtb;

void bp(void);

static void TM_Think(GOBJ *_) {
    HSD_Pad *pad = PadGetMaster(0);
    if (state == ST_Init && (pad->down & HSD_BUTTON_A)) {
        OSReport("setting character\n");
        SlpEXITransferBuffer(pstb, sizeof(*pstb), SlpExiWrite);

        OSReport("finding match\n");
        SlpEXITransferBuffer(fmtb, sizeof(*fmtb), SlpExiWrite);

        state = ST_Searching;
    }
    else if (state == ST_Searching) {
        // use msrb to send 
        msrb->connection_state = SlippiCmdGetMatchState;
        SlpEXITransferBuffer(msrb, 1, SlpExiWrite);
        SlpEXITransferBuffer(msrb, sizeof(*msrb), SlpExiRead);

        OSReport("%u %u!\n", msrb->is_local_player_ready, msrb->is_remote_player_ready);
        
        if (msrb->is_local_player_ready && msrb->is_remote_player_ready) {
            (*stc_css_minorscene)->exit_kind = 1;
            memcpy(&(*stc_css_minorscene)->vs_data.match_init, &msrb->game_info_block, sizeof(MatchInit));
            OSReport("hacking scene change!\n");
            stc_scene_info->major_curr = 8; // hack!
            stc_scene_info->minor_next = 2;

            Preload *preload = Preload_GetTable();
            preload->queued.fighters[0].kind = msrb->game_info_block.playerData[0].c_kind;
            preload->queued.fighters[0].costume = msrb->game_info_block.playerData[0].costume;
            preload->queued.fighters[1].kind = msrb->game_info_block.playerData[1].c_kind;
            preload->queued.fighters[1].costume = msrb->game_info_block.playerData[1].costume;
        }
    }
    
    // TEMP
    /*HSD_Pad *pad = PadGetMaster(0);
    if (pad->down & HSD_BUTTON_A) {
        msrb->connection_state = SlippiCmdGetMatchState;
        SlpEXITransferBuffer(msrb, 1, SlpExiWrite);
        SlpEXITransferBuffer(msrb, sizeof(*msrb), SlpExiRead);

        // msrb->game_info_block.playerData[0].c_kind = CKIND_FOX;
        // msrb->game_info_block.playerData[0].costume = 0;
        // msrb->game_info_block.playerData[1].c_kind = CKIND_GAW;
        // msrb->game_info_block.playerData[1].costume = 0;
        
        // Match_EndVS();

        // (*stc_css_minorscene)->exit_kind = 2;
        Match_EndImmediate();
        // memcpy(&(*stc_css_minorscene)->vs_data.match_init, &msrb->game_info_block, sizeof(MatchInit));
        OSReport("hacking scene change!\n");
        // stc_scene_info->major_curr = 8; // hack!
        // stc_scene_info->minor_next = 2;

        // Preload *preload = Preload_GetTable();
        // preload->queued.fighters[0].kind = msrb->game_info_block.playerData[0].c_kind;
        // preload->queued.fighters[0].costume = msrb->game_info_block.playerData[0].costume;
        // preload->queued.fighters[1].kind = msrb->game_info_block.playerData[1].c_kind;
        // preload->queued.fighters[1].costume = msrb->game_info_block.playerData[1].costume;
    }*/
}

/*
    TODO: two paths:
        1. change scene to the slippi major scene and challenger intro minor scene after connecting.
        2. reconstruct alllll the logic so that we can control everything.
    1 is definitely quickest, but is certainly the most hacky of any option.
    I would need to forcibly change the major scene index which the game prolly doesn't like.
    2 would be more flexible for sure.
*/

static void TestSceneDecide(void) {
    OSReport("ASKJDHKAJSHDJKAH\n");
}

extern MajorSceneDesc stc_scene_table_major[];
extern MinorSceneDesc stc_scene_table_minor[];

static void OnTrainingMode(void) {
    OSReport("Enter training mode\n");

    pstb = HSD_MemAlloc(sizeof(*pstb));
    msrb = HSD_MemAlloc(sizeof(*msrb));
    fmtb = HSD_MemAlloc(sizeof(*fmtb));
    
        // MajorSceneDesc *major = &stc_scene_table_major[MJRKIND_TRAIN];
    for (int j = 0; j < MJRKIND_NULL; ++j) {
        MajorSceneDesc *major = &stc_scene_table_major[j];
        for (int i = 0; ; ++i) {
            MinorScene *minor = &major->minor_scene_arr[i];
            if (minor->minor_id == -1) break;
            // if (minor->minor_kind == MNRKIND_MATCH) {
                minor->minor_decide = TestSceneDecide;
            // }
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

