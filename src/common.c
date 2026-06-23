#define countof(A) (sizeof(A)/sizeof(*(A)))

typedef struct Rect {
    f32 x0, y0, x1, y1;
} Rect;

// ANIM ---------------------------------------------------------------

typedef struct Anim {
    f32 cur, target;
    f32 speed, exp;
    f32 start, end;
} Anim;

static void Anim_Start(Anim *anim) { anim->target = anim->end; }
static void Anim_StartRev(Anim *anim) { anim->target = anim->start; }

static bool Anim_Update(Anim *anim) {
    if (anim->cur == anim->target) return true;
    f32 delta = anim->target - anim->cur;
    f32 delta_sign = sign(delta);
    f32 change = anim->speed * delta_sign + anim->exp * delta;
    if (fabs(change) < fabs(delta)) {
        anim->cur += change;
        return false;
    } else {
        anim->cur = anim->target;
        return true;
    }
}

// BASE GFX ---------------------------------------------------------------

typedef struct GFX_Params {
    u8 shape; // GX_TRIANGLES, GX_LINES, etc.
    u8 size; // width of lines and points

    // more fields may be added in the future
} GFX_Params;

static void GFX_Start(u16 vtx_count, GFX_Params params)
{
    // mostly copy pasted from 80391A04
    Mtx mtx;
    HSD_ClearVtxDesc();
    GXSetCurrentMtx(0);
    COBJ_GetViewingMtx(COBJ_GetCurrent(), &mtx);
    GXLoadPosMtxImm(mtx, 0);
    HSD_StateSetLineWidth(params.size, 5);
    HSD_StateSetPointSize(params.size, 5);
    HSD_SetupRenderMode(0x68000002);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    HSD_StateSetCullMode(0);
    COBJ_GetViewingMtx(COBJ_GetCurrent(), &mtx);

    GXBegin(params.shape, GX_VTXFMT0, vtx_count);
}

// EXPECTS PREMULTIPLIED COLORS
static inline void GFX_AddVtx(f32 x, f32 y, f32 z, GXColor color) {
    gx_pipe->d.F32 = x;
    gx_pipe->d.F32 = y;
    gx_pipe->d.F32 = z;
    gx_pipe->d.U8 = color.r;
    gx_pipe->d.U8 = color.g;
    gx_pipe->d.U8 = color.b;
    gx_pipe->d.U8 = color.a;
}

// HUD ---------------------------------------------------------------


// gxlink:
// 0 in-game
// 3 4 5 6 7 in-game on top
// 8 10 hud no xlu
// 9 14 16 weird
// 11 hud xlu
// 12 hud disable on pause 
// 15 hud xlu on top
// 17 double weird
#define GXLINK_HUD 15

static GOBJ *hud_gobj;
static int hud_canvas;

static Text *hud_text_cache[64];
static u32 hud_text_cache_used;
static void HUD_TextCacheReset(void) {
    for (u32 i = 0; i < hud_text_cache_used; ++i)
        hud_text_cache[i]->hidden = true;
    hud_text_cache_used = 0;
}

static void HUD_Init(void) {
    hud_gobj = GObj_Create(0, 0, 0);
    hud_canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 20, 0);
}

static COBJ *hud_camera_backup;
static void HUD_StartGX(void) {
    HUD_TextCacheReset();
    hud_camera_backup = COBJ_GetCurrent();
    CObj_SetCurrent(hud_gobj->hsd_object);
}

static void HUD_EndGX(void) {
    CObj_SetCurrent(hud_camera_backup);
}

static void HUD_DrawRects(Rect *rects, GXColor *colors, int count) {
    GFX_Start(count * 4, (GFX_Params) { .shape = GX_QUADS });
    for (int i = 0; i < count; ++i) {
        GXColor color = colors[i];

        Rect *rect = &rects[i];
        f32 x0 = rect->x0;
        f32 y0 = rect->y0;
        f32 x1 = rect->x1;
        f32 y1 = rect->y1;

        GFX_AddVtx(x0, y0, 0.f, color);
        GFX_AddVtx(x0, y1, 0.f, color);
        GFX_AddVtx(x1, y1, 0.f, color);
        GFX_AddVtx(x1, y0, 0.f, color);
    }
}

static void HUD_DrawText(
    const char *text, Rect *pos, f32 size,
    GXColor text_color, bool align
) {
    // skip if already used up entire text cache
    if (hud_text_cache_used == countof(hud_text_cache))
        return;

    // create text if it doesn't exist
    Text **text_ptr = &hud_text_cache[hud_text_cache_used++];
    if (*text_ptr == 0) {
        Text *new_text = Text_CreateText(2, hud_canvas);
        *text_ptr = new_text;
        new_text->kerning = 1;
        new_text->align = align;
        new_text->viewport_scale.X = 0.1f;
        new_text->viewport_scale.Y = 0.1f;
        Text_AddSubtext(new_text, 0, 0, "");
    }

    Text *hud_text = *text_ptr;
    Text_SetColor(hud_text, 0, &text_color);

    hud_text->align = align;
    hud_text->hidden = false;
    f32 w = pos->x1 - pos->x0;
    f32 h = pos->y1 - pos->y0;

    f32 x = pos->x0 * 10.f;
    x += align ? w * 5.f : 10.f;
    f32 y = pos->y0 * -10.f + h * -5.f - 25.f;

    Text_SetText(hud_text, 0, text);
    Text_SetScale(hud_text, 0, size, size);
    Text_SetPosition(hud_text, 0, x, y);
}

static void HUD_DrawMenuItem(Rect *pos, GXColor outline, GXColor inside) {
    static const u8 outline_size = 8;
    static const f32 t = (f32)outline_size * 0.008f;
    static const f32 midpoint = 0.65f; // placement of midpoint
    static const f32 slope = 0.3f; // the slope of midpoint -> top/bottom
    
    f32 x0 = pos->x0;
    f32 y0 = pos->y0;
    f32 x1 = pos->x1;
    f32 y1 = pos->y1;

    f32 ml_x = x0;
    f32 ml_y = y0 + (y1 - y0) * midpoint;
    f32 tl_x = x0 + slope*(y1 - ml_y);
    f32 tl_y = y1;
    f32 mr_x = x1;
    f32 mr_y = y0 + (y1 - y0) * (1.f-midpoint);
    f32 tr_x = x1 - slope*(y1 - mr_y);
    f32 tr_y = y1;
    f32 br_x = x1 - slope*(mr_y - y0);
    f32 br_y = y0;
    f32 bl_x = x0 + slope*(ml_y - y0);
    f32 bl_y = y0;
    
    GFX_Start(6, (GFX_Params) { .shape = GX_TRIANGLESTRIP });
    GFX_AddVtx(ml_x, ml_y, 0, inside); // mid left
    GFX_AddVtx(bl_x, bl_y, 0, inside); // bottom left
    GFX_AddVtx(tl_x, tl_y, 0, inside); // top left
    GFX_AddVtx(br_x, br_y, 0, inside); // bottom right
    GFX_AddVtx(tr_x, tr_y, 0, inside); // top right
    GFX_AddVtx(mr_x, mr_y, 0, inside); // mid right

    // first round for shitty antialiasing:
    GXColor outline_aa = outline;
    outline_aa.r >>= 1; outline_aa.g >>= 1; outline_aa.b >>= 1; outline_aa.a >>= 1;
    GFX_Start(8, (GFX_Params) { .shape = GX_LINES, .size=outline_size+2 });
    GFX_AddVtx(bl_x, bl_y, 0, outline_aa); // bottom left
    GFX_AddVtx(ml_x, ml_y, 0, outline_aa); // mid left
    GFX_AddVtx(ml_x, ml_y, 0, outline_aa); // mid left
    GFX_AddVtx(tl_x, tl_y, 0, outline_aa); // top left
    GFX_AddVtx(tr_x, tr_y, 0, outline_aa); // top right
    GFX_AddVtx(mr_x, mr_y, 0, outline_aa); // mid right
    GFX_AddVtx(mr_x, mr_y, 0, outline_aa); // mid right
    GFX_AddVtx(br_x, br_y, 0, outline_aa); // bottom right

    // second round for actual outline
    GFX_Start(12, (GFX_Params) { .shape = GX_LINES, .size=outline_size });
    GFX_AddVtx(bl_x, bl_y, 0, outline); // bottom left
    GFX_AddVtx(ml_x, ml_y, 0, outline); // mid left

    GFX_AddVtx(ml_x, ml_y, 0, outline); // mid left
    GFX_AddVtx(tl_x, tl_y, 0, outline); // top left

    // since lines end parallel to the nearest horizontal/vertical,
    // we need to drop down the horizontal lines to hide the gap
    GFX_AddVtx(tl_x, tl_y-t, 0, outline); // top left
    GFX_AddVtx(tr_x, tr_y-t, 0, outline); // top right
    GFX_AddVtx(bl_x, bl_y+t, 0, outline); // bottom left
    GFX_AddVtx(br_x, br_y+t, 0, outline); // bottom right

    GFX_AddVtx(br_x, br_y, 0, outline); // bottom left
    GFX_AddVtx(mr_x, mr_y, 0, outline); // mid left

    GFX_AddVtx(mr_x, mr_y, 0, outline); // mid left
    GFX_AddVtx(tr_x, tr_y, 0, outline); // top left
}

// MENU ----------------------------------------------------------------------

static const GXColor menu_colour_default_outline = {230, 230, 230, 255};
static const GXColor menu_colour_default_inside = {0, 0, 0, 180};
static const GXColor menu_colour_default_text = {230, 230, 230, 255};
static const GXColor menu_colour_default_subtext = {190, 190, 190, 255};
static const GXColor menu_colour_enabled_outline = {80, 255, 70, 255};
static const GXColor menu_colour_enabled_inside = {50, 70, 50, 200};

typedef struct MenuItem {
    const char *name;
    void (*Click)(struct MenuItem *self);
    int state;
    GXColor inside_color;
    GXColor outline_color;
    GXColor text_color;
} MenuItem;

typedef struct Menu {
    s32 item_count;
    MenuItem *items;
    void (*Think)(struct Menu *self);
    void (*GX)(struct Menu *self);
    s32 selected_idx_cur;
    s32 selected_idx_prev;
    Anim scroll;
} Menu;

static Rect MenuRect(f32 x, f32 y) {
    return (Rect) {
        x - 10.f, y - 1.5f,
        x + 10.f, y + 1.5f,
    };
}

static void MenuItem_Off(MenuItem *item) {
    item->inside_color = (GXColor) {0};
    item->outline_color = (GXColor) {0};
    item->text_color = (GXColor) {0};
    item->state = false;
}

static void MenuItem_On(MenuItem *item) {
    item->outline_color = menu_colour_enabled_outline;
    item->inside_color = menu_colour_enabled_inside;
    item->text_color = (GXColor) {0};
    item->state = true;
}

static bool MenuItem_Toggle(MenuItem *item) {
    if (item->state) {
        MenuItem_Off(item);
        return false;
    } else {
        MenuItem_On(item);
        return true;
    }
}

// TEXT INPUT -----------------------------------------------------

static char text_input_chars_l[15] = "QWERTASDFGZXCVB";
static char text_input_chars_l_alt[15] = "1234567890";
static char text_input_chars_r[15] = "YUIOPHJKL#NM012";

static const char text_input_map_l[24] =
    "DERTGF" // top right
    "DCVBGF" // bottom right
    "DEWQAS" // top left
    "DCXZAS" // bottom left
;

static const char text_input_map_l_alt[24] =
    "834509" // top right
    "8   09" // bottom right
    "832167" // top left
    "8   67" // bottom left
;

static const char text_input_map_r[24] =
    "KIOP#L" // top right
    "K012#L" // bottom right
    "KIUYHJ" // top left
    "K0MNHJ" // bottom left
;

static const u32 text_input_quad_row[10] = {
   // octal :)
   01222222233,
   01222222333,
   01222223333,
   01222233333,
   01222233333,
   01222233333,
   01555555333,
   00055555554,
   00055555554,
   00055555554,
};

static u32 TextInput_CoordIdx(s8 x, s8 y) {
    // fold quadrants
    u32 idx = 0;
    if (x < 0) { x = -x; idx += 12; }
    if (y < 0) { y = -y; idx += 6; }

    // group by 8 units and find row
    u32 row = text_input_quad_row[(79-y) / 8];

    // group by 8 units and bit manip to extract octal
    idx += (row >> ((79 - x) / 8)*3) & 0b111;
    return idx;
}
static char TextInput_LStickChar(s8 x, s8 y) {
    return text_input_map_l[TextInput_CoordIdx(x, y)];
}

static char TextInput_LStickChar_Alt(s8 x, s8 y) {
    return text_input_map_l_alt[TextInput_CoordIdx(x, y)];
}

static char TextInput_CStickChar(s8 x, s8 y) {
    return text_input_map_r[TextInput_CoordIdx(x, y)];
}
