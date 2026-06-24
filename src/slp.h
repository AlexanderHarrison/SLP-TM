#include "../MexTK/mex.h"

#ifndef HEADER_ONLINE
#define HEADER_ONLINE

// For Slippi communication
#define SlippiCmdGetFrame 0x76
#define SlippiCmdCheckForReplay 0x88
#define SlippiCmdCheckForStockSteal 0x89
#define SlippiCmdSendOnlineFrame 0xB0
#define SlippiCmdCaptureSavestate 0xB1
#define SlippiCmdLoadSavestate 0xB2
#define SlippiCmdGetMatchState 0xB3
#define SlippiCmdFindOpponent 0xB4
#define SlippiCmdSetMatchSelections 0xB5
#define SlippiCmdOpenLogIn 0xB6
#define SlippiCmdLogOut 0xB7
#define SlippiCmdUpdateApp 0xB8
#define SlippiCmdGetOnlineStatus 0xB9
#define SlippiCmdCleanupConnections 0xBA
#define SlippiCmdSendChatMessage 0xBB
#define SlippiCmdGetNewSeed 0xBC
#define SlippiCmdReportMatch 0xBD
#define SlippiCmdAutoComplete 0xBE
#define SlippiCmdReportSetCompletion 0xC2
#define SlippiCmdReportMatchStatus 0xC4
#define SlippiCmdGetRank 0xE3
#define SlippiCmdFetchRank 0xE4

// For Slippi file loads
#define SlippiCmdFileLength 0xD1
#define SlippiCmdFileLoad 0xD2
#define SlippiCmdGctLength 0xD3
#define SlippiCmdGctLoad 0xD4

// Misc
#define SlippiCmdGetDelay 0xD5
#define SlippiPlayMusic 0xD6
#define SlippiStopMusic 0xD7
#define SlippiChangeMusicVolume 0xD8

// For Slippi Premade Texts
#define SlippiCmdGetPremadeTextLength 0xE1
#define SlippiCmdGetPremadeText 0xE2

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Offsets from r13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define OFST_R13_ODB_ADDR R13_OFFSET(-0x49e4) // Online data buffer
#define OFST_R13_ONLINE_MODE R13_OFFSET(-0x5060) // Byte Selected online mode
#define OFST_R13_APP_STATE R13_OFFSET(-0x505F) // Byte App state / online status
#define OFST_R13_FORCE_MENU_CLEAR R13_OFFSET(-0x505E) // Byte Force menu clear
#define OFST_R13_NAME_ENTRY_MODE R13_OFFSET(-0x505D) // Byte 0 = normal 1 = connect code
#define OFST_R13_SWITCH_TO_ONLINE_SUBMENU R13_OFFSET(-0x49ec) // Function used to switch
#define OFST_R13_CALLBACK R13_OFFSET(-0x5018) // Callback address
#define OFST_R13_ISPAUSE R13_OFFSET(-0x5038) // byte client paused bool (-originally used for tournament mode @ 8019b8e4)
#define OFST_R13_ISWINNER R13_OFFSET(-0x5037) // byte used to know if the player won the previous match
#define OFST_R13_CHOSESTAGE R13_OFFSET(-0x5036) // bool used to know if the player has selected a stage
#define OFST_R13_NAME_ENTRY_INDEX_FLAG R13_OFFSET(-0x5035) // byte set to 1 if just entered name entry. Rsts direct code index.
#define OFST_R13_USE_PREMADE_TEXT R13_OFFSET(-0x5014) // bool used to make Text_CopyPremadeTextDataToStruct load text data from dolphin
#define OFST_R13_ISWIDESCREEN R13_OFFSET(-0x5020) // bool used to make Text_CopyPremadeTextDataToStruct load text data from dolphin

#define CSSDT_BUF_ADDR ((CSSDataTable*)0x80005614)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CONTROLLER_COUNT 4
#define PAD_REPORT_SIZE 0xC
#define MIN_DELAY_FRAMES 1
#define MAX_DELAY_FRAMES 15
#define ROLLBACK_MAX_FRAME_COUNT 7
#define LGL_LIMIT 45 // Ledge grabs that exceed this number will result in a loss on timeout

// I don't know exactly how long the local input buffer has to be but in very rare cases with a length
// of ROLLBACK_MAX_FRAME_COUNT we could overflow into the negative indices:
// [3975] Prior to local input copy. END_FRAME: 3983 LOCAL_INPUTS_IDX: 0
// [3975] Copying local inputs for rollback. Idx: -2 Offset: -24
#define LOCAL_INPUTS_COUNT 2 * ROLLBACK_MAX_FRAME_COUNT

// Predicted input buffer might be able to bet ROLLBACK_MAX_FRAME_COUNT long but I'm not sure
#define PREDICTED_INPUTS_COUNT 2 * ROLLBACK_MAX_FRAME_COUNT

// This one should be fine Dolphin caps how many inputs it sends to the rollback limit
#define RXB_INPUTS_COUNT ROLLBACK_MAX_FRAME_COUNT

#define UNFREEZE_INPUTS_FRAME 84

// Inputs before freeze time are important because if they get zero'd out inputs on the first
// actionable frame will be treated as new inputs rather than held inputs.
// Think 5 should be more than enough (pad buffer size) 6 to be safe
#define START_SYNC_FRAME UNFREEZE_INPUTS_FRAME - 6

#define STATIC_PLAYER_BLOCK_P1 0x80453080
#define STATIC_PLAYER_BLOCK_LEN 0xE90

#define MATCH_STRUCT_LEN 0x138

#define ONLINE_MODE_RANKED 0
#define ONLINE_MODE_UNRANKED 1
#define ONLINE_MODE_DIRECT 2
#define ONLINE_MODE_TEAMS 3
#define ONLINE_MODE_PARTY 4

#define OPTION_RANKED_IDX 0
#define OPTION_UNRANKED_IDX 1
#define OPTION_DIRECT_IDX 2
#define OPTION_TEAMS_IDX 3
#define OPTION_PARTY_IDX 4
#define OPTION_LOGIN_IDX 5
#define OPTION_LOGOUT_IDX 6
#define OPTION_UPDATE_IDX 7

#define ONLINE_SUBMENU_OPTION_COUNT 8

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Online Scenes
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SCENE_ONLINE_CSS 0x0008
#define SCENE_ONLINE_SSS 0x0108
#define SCENE_ONLINE_IN_GAME 0x0208
#define SCENE_ONLINE_RESULTS 0x0308
#define SCENE_ONLINE_VS 0x0408

/*
-each is 0xC long
-4 total one for each controller

0x0 = u8 ---SYXBA
0x1 = u8 -LRZUDRL
0x2 = int8 leftstick X
0x3 = int8 leftstick Y
0x4 = int8 rightstick X
0x5 = int8 rightstick Y
0x6 = int8 lefttrigger value
0x7 = int8 righttrigger value
0x8 = int8 unk
0x9 = int8 unk
0xA = int8 isConnected (0 = connected -1 = disconnected -3 = stale?)
0xB = padding
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ISWINNER Values
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ISWINNER_NULL -1    // indicates this is the first match / no previous winner
#define ISWINNER_LOST 0    // indicates the player lost the previous match
#define ISWINNER_WON 1    // indicates the player won the previous match

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stage Behavior Arg Values (r3 for FN_LOCK_IN_AND_SEARCH and FN_TX_LOCK_IN)
// 0+ = specify stage ID.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define  SB_RAND -2
#define  SB_NOTSEL -1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Savestate Request Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 cmd;
    u32 frame;
    void *odb;
    u32 odb_size;
    void *rxb;
    u32 rxb_size;
    void *sscb;
    u32 sscb_size;
    u32 terminator;
} SavestateRequestBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Savestate Data Buffer - Includes data stored locally for savestates
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u32 frame;
} SavestateDataBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Savestate Pre-Load Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u32 sound_entry_id;
    u32 sound_entry_loc;
} SavestatePreLoadBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Savestate Control Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u32 write_index; // next index to write savestate at
    u8 ssdb_count;
    SavestateDataBuffer ssdb[ROLLBACK_MAX_FRAME_COUNT];
    SavestatePreLoadBuffer ssplb[16];
} SavestateControlBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SFX Storage
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MAX_SOUNDS_PER_FRAME 0x10

// The entry is the data needed to keep track of for a given sound every frame
typedef struct __attribute__((packed)) {
    u32 sound_id;
    u32 instance_id;
} SFXSEntry;

// A log keeps tracks of sounds on a given frame the index is effectively how
// many sounds have been encountered so far
typedef struct __attribute__((packed)) {
    u8 log_index;
    SFXSEntry entries[MAX_SOUNDS_PER_FRAME];
} SFXSLog;

// Pending log keeps track of all inputs during execution of a frame
// Stable log is copied from the pending log at the end of the frame
// Both are needed because as a frame is processing we need to check if the last
// time this frame was played whether our sound was executed. This is the purpose
// of the stable log the pending log is needed to keep track of the latest frame
// sound execution such that on the next frame it can be used as the stable log
typedef struct __attribute__((packed)) {
    SFXSLog frame_pending_log;
    SFXSLog frame_stable_log;
} SFXSFrame;

// Write index increments every frame we process
typedef struct __attribute__((packed)) {
    u8 write_index;
    SFXSFrame frames[ROLLBACK_MAX_FRAME_COUNT];
} SFXDB;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Desync Recovery
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 stocks_remaining;
    u16 percent;
} DesyncFighterRecoveryEntry;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Desync Detection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    s32 frame;
    u32 checksum;
} DesyncDetectionRemoteEntry;

typedef struct __attribute__((packed)) {
    s32 frame;
    u32 checksum;
    u32 recovery_timer;
    DesyncFighterRecoveryEntry recovery_fighter_arr[4];
} DesyncDetectionLocalEntry;

// I'm not exactly sure how many local entries we need to keep but our local entries will get
// compared with the opponents' last stabilized frame which with a lot of ping can come pretty late.
// My guess would be we could so 2 * ROLLBACK_MAX_FRAME_COUNT but 3 should definitely be safe
#define DESYNC_ENTRY_COUNT ROLLBACK_MAX_FRAME_COUNT * 3

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Online Data Buffer Offsets
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct __attribute__((packed)) {
    u8 cmd;
    s32 frame;
    s32 finalized_frame;
    u32 finalized_frame_checksum;
    u8 delay;
    u8 pad[PAD_REPORT_SIZE];
} TXB;

typedef struct __attribute__((packed)) {
    u8 result;
    u8 opnt_count;
    DesyncDetectionRemoteEntry opnt_desync_entry[3];
    s32 opnt_frame_nums[3];
    s32 smallest_latest_frame;
    bool should_despawn[3];
} RXB;

typedef struct __attribute__((packed)) {
    u8      local_player_index;
    u8      online_player_index;
    u8      input_source_index;
    u32     frame;
    u32     rng_offset;
    u32     game_end_frame;
    bool    is_game_over;
    bool    is_disconnected;
    bool    is_disconnect_state_displayed;
    bool    is_desync_state_displayed;
    bool    is_desync_risk_displayed;
    bool    is_frame_advance;
    u8      last_local_inputs[PAD_REPORT_SIZE];
    u8      delay_frames;
    u8      delay_buffer_index;
    u8      delay_buffer[PAD_REPORT_SIZE * MAX_DELAY_FRAMES];
    TXB    *txb;
    RXB    *rxb;
    bool    rollback_is_active;
    bool    rollback_should_load_state;
    s32     rollback_end_frame;
    u8      rollback_local_inputs_idx;
    u8      rollback_local_inputs[PAD_REPORT_SIZE * LOCAL_INPUTS_COUNT];
    u8      rollback_predicted_inputs_read_idxs[3];
    u8      rollback_predicted_inputs_write_idxs[3];
    u8      rollback_predicted_inputs[PAD_REPORT_SIZE * PREDICTED_INPUTS_COUNT * 3];
    bool    savestate_is_predicting;
    s32     savestate_frame;
    u32     player_savestate_frame[3];
    u8      player_savestate_is_predicting[3];
    SavestateRequestBuffer *ssrb;
    SavestateControlBuffer *sscb;
    SFXDB   sfxdb;
    u32     latest_frame;
    u32     fn_handle_game_over_addr;
    bool    stable_rollback_is_active;
    s32     stable_rollback_end_frame;
    bool    stable_rollback_should_load_state;
    s32     stable_savestate_frame;
    s32     stable_finalized_frame;
    bool    should_force_pad_renew;
    u32     hud_canvas;
    u32     hud_text_struct;
    u32     pause_counter;
    u32     finalized_frame;
    u32     rest_stick_change_counter;
    u32     local_desync_last_frame;
    u8      local_desync_write_idx;
    DesyncDetectionLocalEntry local_desync_arr[DESYNC_ENTRY_COUNT];
    u32     desync_recovery_timer;
    DesyncFighterRecoveryEntry desync_recovery_fighter_arr[4];
} OnlineDataBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Matchmaking States
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MM_STATE_IDLE 0
#define MM_STATE_INITIALIZING 1
#define MM_STATE_MATCHMAKING 2
#define MM_STATE_OPPONENT_CONNECTING 3
#define MM_STATE_CONNECTION_SUCCESS 4
#define MM_STATE_ERROR_ENCOUNTERED 5

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Match State Response Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 connection_state;
    bool is_local_player_ready;
    bool is_remote_player_ready;
    u8 local_player_index;
    u8 remote_player_index;
    u32 rng_offset;
    u8 delay_frames;
    u8 user_chatmsg_id;
    u8 opp_chatmsg_id;
    u8 chatmsg_player_index;
    u8 user_rank;
    u8 opp_rank;
    char local_name[31];
    char p1_name[31];
    char p2_name[31];
    char p3_name[31];
    char p4_name[31];
    char opp_name[31];
    char p1_connect_code[10];
    char p2_connect_code[10];
    char p3_connect_code[10];
    char p4_connect_code[10];
    char p1_slippi_uid[29];
    char p2_slippi_uid[29];
    char p3_slippi_uid[29];
    char p4_slippi_uid[29];
    char error_msg[241];
    MatchInit game_info_block;
    char match_id[51];
    u8 alt_stage_mode;
} MatchStateResponseBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rank Info Response Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 visibility;
    u8 status;
    u8 rank;
    float rating_ordinal;
    u8 global_placing;
    u8 regional_placing;
    u32 update_count;
    float rating_change;
    s32 rank_change;
} RankInfoResponseBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Player Selections Transfer Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 cmd;
    u8 team_id;
    u8 char_id;
    u8 char_color;
    u8 char_opt; // 0 = unset, 1 = merge, 2 = clear
    u16 stage_id;
    u8 stage_opt; // 0 = unset, 1 = merge, 2 = clear, 3 = random
    u8 online_mode;
    u8 alt_stage_mode;
} PlayerSelectionsTransferBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Chat Messages Transfer Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 cmd;
    u8 message;
} ChatMessagesTransferBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Find Match Transfer Buffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 cmd;
    u8 online_mode;
    char opp_connect_code[18];
} FindMatchTransferBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// slpCSS Symbol Structure
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u32 chat_select;
    u32 chat_msg;
    u32 mode;
    u32 connect_help;
} SlpCSSSymbolStructure;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSS Data Table
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    MatchStateResponseBuffer *msrb_addr;
    SlpCSSSymbolStructure *slpcss_addr;
    Text *text_struct;
    u8 spinner_1; // 0 = hide, 1 = spin, 2 = done
    u8 spinner_2; // 0 = hide, 1 = spin, 2 = done
    u8 spinner_3; // 0 = hide, 1 = spin, 2 = done
    u16 frame_counter;
    bool prev_lock_in_state;
    u8 prev_connected_state;
    u8 z_button_hold_timer;
    u8 chat_window_opened;
    u16 chat_last_input;
    u8 chat_msg_count;
    u8 chat_local_msg_count;
    u8 last_chat_msg_index;
    u8 team_idx;
    u8 team_costume_idx;
} CSSDataTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSS Chat Message Data Table
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 timer;
    u8 timer_status;
    u8 msg_id;
    u8 msg_index;
    Text *msg_text_struct;
    u8 player_index;
    CSSDataTable *cssdt;
} CSSChatMessageDataTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSS Team Icon Button Data Table
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    CSSDataTable *cssdt;
} CSSTeamIconButtonDataTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSS Chat Window Data Table
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 input;
    bool input_sent;
    bool timer;
    Text *text_struct;
    CSSDataTable *cssdt;
} CSSChatWindowDataTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Auto Complete Transfer Buffer 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SLIPPI_SCROLL_NONE 0
#define SLIPPI_SCROLL_OLDER 1
#define SLIPPI_SCROLL_NEWER 2
#define SLIPPI_SCROLL_RESET 3

typedef struct __attribute__((packed)) {
    u8 cmd;
    char current_text[24];
    u8 current_text_len;
    u32 scroll_index;
    u8 scroll_direction;
    u8 online_mode;
} AutoCompleteTransferBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Auto Complete Receive Buffer 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 has_suggestion;
    char completed_text[24];
    u8 completed_text_len;
    u32 new_scroll_index;
} AutoCompleteReceiveBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Online status buffer offsets
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 app_state; // 0 = not logged in, 1 = logged in, 2 = update required
    char player_name[31];
    char connect_code[10];
} OnlineStatusBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define report game buffer offsets and length
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 slot_type; // 0 = Human, 1 = CPU, 2 = Demo, 3 = Empty
    u8 stocks_remaining;
    float damage_done;
    u8 synced_stocks;
    u16 synced_damage;
} ReportGamePlayerBuffer;

typedef struct __attribute__((packed)) {
    u8 cmd;
    u8 online_mode;
    u32 frame_length;
    u32 game_index; // 1-indexed
    u32 tiebreaker_index; // 1-indexed, 0 = not tiebreak
    s8 winner_idx;
    u8 game_end_method;
    s8 lras_initiator;
    u32 synced_timer;
    ReportGamePlayerBuffer rgbp[4];
    MatchInit game_info_block;
} ReportGameBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define game prep data and include macro to create static data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct __attribute__((packed)) {
    u8 max_games;
    u16 cur_game;
    u8 score_by_player[2];
    u8 prev_winner;
    u8 tiebreak_game_num;
    u8 game_results[9];
    u16 last_stage_win_by_player[2];
    bool color_ban_active;
    u8 color_ban_char;
    u8 color_ban_color;
    u8 last_game_end_mode;
    void *fn_compute_ranked_winner;
} GamePrepData;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define online static data (OSD) and include macro to create static data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define OSD_LOCAL_PLAYER_INDEX 0 // u8

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Const values
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define RESP_NORMAL 1
#define RESP_SKIP 2
#define RESP_DISCONNECTED 3
#define RESP_ADVANCE 4

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Slippi Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum {
    SlpExiRead = 0,
    SlpExiWrite = 1,
} SlpEXITransferMode;

void SlpEXITransferBuffer(void *buf, u32 length, SlpEXITransferMode mode);
void SlpRequestSSM(u32 ssm_idx);
void SlpCaptureSavestate(SavestateRequestBuffer *request, u32 frame_idx, SavestateControlBuffer *control);
void SlpLoadSavestate(SavestateRequestBuffer *request, u32 frame_idx, SavestateControlBuffer *control);

#endif
