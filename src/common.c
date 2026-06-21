#define countof(A) (sizeof(A)/sizeof(*(A)))

typedef struct Rect {
    f32 x0, y0, x1, y1;
} Rect;

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
