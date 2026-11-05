// CREATURE QUEST - an original monster-catching RPG for Game Boy Advance
// No libraries needed. Mode 3 (240x160 bitmap), direct hardware registers.
//
// Build (devkitARM):
//   arm-none-eabi-gcc -O2 -mthumb -mthumb-interwork -specs=gba.specs main.c -o game.elf
//   arm-none-eabi-objcopy -O binary game.elf game.gba
//   gbafix game.gba
//
// Controls: D-pad move/select, A confirm, B back, START party screen.

typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned char u8;

#define REG_DISPCNT (*(volatile u16 *)0x04000000)
#define REG_VCOUNT  (*(volatile u16 *)0x04000006)
#define REG_KEYS    (*(volatile u16 *)0x04000130)
#define VRAM        ((volatile u16 *)0x06000000)
#define W 240
#define H 160
#define RGB(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
#define WHITE RGB(31, 31, 31)

enum { K_A = 1, K_B = 2, K_SEL = 4, K_START = 8, K_R = 16, K_L = 32, K_U = 64, K_D = 128 };
// real register bit order: A,B,SEL,START,RIGHT,LEFT,UP,DOWN
#define KEY_RIGHT 16
#define KEY_LEFT 32
#define KEY_UP 64
#define KEY_DOWN 128

/* ---------- basics ---------- */
static u16 prevk;
static u32 seed = 12345;
static u32 rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 16; }
static void vsync(void) { while (REG_VCOUNT >= 160); while (REG_VCOUNT < 160); }
static u16 keys(void) { return (~REG_KEYS) & 0x3FF; }
static u16 pressed(void) { u16 k = keys(); u16 p = k & ~prevk; prevk = k; return p; }
static u16 waitpress(u16 mask) {
  for (;;) { vsync(); seed++; u16 p = pressed(); if (p & mask) return p & mask; }
}

/* ---------- drawing ---------- */
static void rect(int x, int y, int w, int h, u16 c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  for (int j = 0; j < h; j++) {
    volatile u16 *p = VRAM + (y + j) * W + x;
    for (int i = 0; i < w; i++) p[i] = c;
  }
}

// 3x5 font, A-Z then 0-9. Each glyph = 5 rows of 3 bits.
#define G(a, b, c, d, e) ((a << 12) | (b << 9) | (c << 6) | (d << 3) | e)
static const u16 font[36] = {
  G(2,5,7,5,5), G(6,5,6,5,6), G(3,4,4,4,3), G(6,5,5,5,6), G(7,4,6,4,7),
  G(7,4,6,4,4), G(3,4,5,5,3), G(5,5,7,5,5), G(7,2,2,2,7), G(1,1,1,5,2),
  G(5,5,6,5,5), G(4,4,4,4,7), G(5,7,7,5,5), G(6,5,5,5,5), G(2,5,5,5,2),
  G(6,5,6,4,4), G(2,5,5,7,3), G(6,5,6,5,5), G(3,4,2,1,6), G(7,2,2,2,2),
  G(5,5,5,5,7), G(5,5,5,5,2), G(5,5,7,7,5), G(5,5,2,5,5), G(5,5,2,2,2),
  G(7,1,2,4,7),
  G(7,5,5,5,7), G(2,6,2,2,7), G(6,1,2,4,7), G(6,1,2,1,6), G(5,5,7,1,1),
  G(7,4,6,1,6), G(3,4,7,5,7), G(7,1,2,2,2), G(7,5,7,5,7), G(7,5,7,1,6)
};

static void chr(int x, int y, char ch, int s, u16 c) {
  int g = -1;
  if (ch >= 'A' && ch <= 'Z') g = ch - 'A';
  else if (ch >= '0' && ch <= '9') g = 26 + ch - '0';
  else if (ch == '!') { rect(x + s, y, s, 3 * s, c); rect(x + s, y + 4 * s, s, s, c); return; }
  else if (ch == '.') { rect(x + s, y + 4 * s, s, s, c); return; }
  else if (ch == '-') { rect(x, y + 2 * s, 3 * s, s, c); return; }
  else if (ch == '/') { rect(x + 2 * s, y, s, 2 * s, c); rect(x + s, y + 2 * s, s, s, c); rect(x, y + 3 * s, s, 2 * s, c); return; }
  if (g < 0) return;
  u16 b = font[g];
  for (int r = 0; r < 5; r++)
    for (int q = 0; q < 3; q++)
      if (b & (1 << (14 - (r * 3 + q)))) rect(x + q * s, y + r * s, s, s, c);
}
static void text(int x, int y, const char *t, int s, u16 c) {
  for (; *t; t++) { chr(x, y, *t, s, c); x += 4 * s; }
}
static char nb[8];
static const char *ntoa(int n) {
  int i = 7; nb[i] = 0;
  if (n <= 0) nb[--i] = '0';
  while (n > 0) { nb[--i] = '0' + n % 10; n /= 10; }
  return nb + i;
}
static void numat(int x, int y, int n, int s, u16 c) { text(x, y, ntoa(n), s, c); }

/* ---------- creatures (all original) ---------- */
typedef struct { const char *name; u8 type; u8 hp, atk; u16 col; } Species;
// types: 0 fire, 1 grass, 2 water, 3 rock, 4 spark, 5 shadow
static const Species sp[6] = {
  { "EMBERPUP", 0, 22, 7, RGB(31, 12, 4) },
  { "LEAFLING", 1, 24, 6, RGB(8, 26, 8) },
  { "AQUABIT",  2, 23, 6, RGB(6, 14, 31) },
  { "PEBBLET",  3, 26, 5, RGB(18, 16, 14) },
  { "ZAPMOTH",  4, 18, 8, RGB(31, 29, 4) },
  { "SHADEKIT", 5, 20, 7, RGB(14, 8, 20) },
};
static const char *mv[6] = { "EMBER", "VINE LASH", "BUBBLE", "ROCK TOSS", "SPARK", "SHADOW CLAW" };

typedef struct { u8 sp, lvl, xp; int hp, maxhp; } Mon;
static Mon party[4];
static int pcount, wins;

static void mk(Mon *m, int s, int l) {
  m->sp = s; m->lvl = l; m->xp = 0;
  m->maxhp = sp[s].hp + l * 3; m->hp = m->maxhp;
}
static int atkv(Mon *m) { return sp[m->sp].atk + m->lvl * 2; }
static int eff(int a, int d) { // 4 strong, 2 normal, 1 weak
  if ((a == 0 && d == 1) || (a == 1 && d == 2) || (a == 2 && d == 0) || (a == 4 && d == 2)) return 4;
  if ((a == 1 && d == 0) || (a == 2 && d == 1) || (a == 0 && d == 2) ||
      (a == 0 && d == 0) || (a == 1 && d == 1) || (a == 2 && d == 2)) return 1;
  return 2;
}
static int firstalive(void) { for (int i = 0; i < pcount; i++) if (party[i].hp > 0) return i; return -1; }
static void healall(void) { for (int i = 0; i < pcount; i++) party[i].hp = party[i].maxhp; }

static void drawmon(int x, int y, int s, u16 c) {
  rect(x, y + 2 * s, 8 * s, 6 * s, c);
  rect(x + s, y, 2 * s, 2 * s, c);
  rect(x + 5 * s, y, 2 * s, 2 * s, c);
  rect(x + 2 * s, y + 3 * s, s, s, WHITE);
  rect(x + 5 * s, y + 3 * s, s, s, WHITE);
  rect(x + 3 * s, y + 6 * s, 2 * s, s, RGB(2, 2, 3));
}
static void bar(int x, int y, int w, int hp, int mx) {
  rect(x, y, w, 5, RGB(8, 8, 8));
  int f = hp * (w - 2) / mx;
  if (hp > 0 && f < 1) f = 1;
  rect(x + 1, y + 1, f, 3, hp * 4 > mx ? RGB(6, 28, 6) : (hp * 8 > mx ? RGB(30, 26, 4) : RGB(30, 6, 4)));
}

/* ---------- text box + menu ---------- */
static char L1[32], L2[32];
static void sset(char *d, const char *a, const char *b) {
  int i = 0;
  while (*a && i < 27) d[i++] = *a++;
  while (b && *b && i < 27) d[i++] = *b++;
  d[i] = 0;
}
static void logbox(void) {
  rect(0, 104, W, 56, RGB(3, 3, 6));
  rect(2, 106, W - 4, 52, WHITE);
  rect(4, 108, W - 8, 48, RGB(3, 3, 6));
}
static void say(const char *a, const char *b) {
  logbox();
  text(10, 114, a, 2, WHITE);
  if (b) text(10, 132, b, 2, WHITE);
  rect(224, 146, 6, 6, WHITE);
  waitpress(K_A | K_B);
}
static int menu(int n, const char **l) {
  int sel = 0;
  for (;;) {
    logbox();
    for (int i = 0; i < n; i++) text(30, 112 + i * 15, l[i], 2, WHITE);
    rect(14, 114 + sel * 15, 6, 6, WHITE);
    u16 p = waitpress(K_A | KEY_UP | KEY_DOWN);
    if (p & K_A) return sel;
    if ((p & KEY_UP) && sel > 0) sel--;
    if ((p & KEY_DOWN) && sel < n - 1) sel++;
  }
}

/* ---------- battle ---------- */
static Mon *bp, *be;

static void battle_draw(void) {
  u16 bg = RGB(24, 29, 22), ink = RGB(2, 2, 3);
  rect(0, 0, W, 104, bg);
  text(8, 8, sp[be->sp].name, 2, ink);
  text(8, 22, "LV", 1, ink); numat(24, 22, be->lvl, 1, ink);
  bar(8, 32, 70, be->hp, be->maxhp);
  drawmon(150, 12, 6, sp[be->sp].col);
  text(124, 70, sp[bp->sp].name, 2, ink);
  text(124, 84, "LV", 1, ink); numat(140, 84, bp->lvl, 1, ink);
  bar(124, 94, 70, bp->hp, bp->maxhp);
  drawmon(20, 50, 6, sp[bp->sp].col);
}

static void attack(Mon *a, Mon *d, int special) {
  sset(L1, a == be ? "WILD " : "", sp[a->sp].name);
  sset(L2, special ? mv[sp[a->sp].type] : "TACKLE", "!");
  say(L1, L2);
  if (special && rnd() % 10 == 0) { say("IT MISSED!", 0); return; }
  int base = atkv(a) / 2 + rnd() % 4 + 2;
  int mult = 2;
  if (special) { base = base * 3 / 2; mult = eff(sp[a->sp].type, sp[d->sp].type); }
  int dmg = base * mult / 2;
  if (dmg < 1) dmg = 1;
  d->hp -= dmg; if (d->hp < 0) d->hp = 0;
  battle_draw();
  if (mult == 4) say("SUPER EFFECTIVE!", 0);
  else if (mult == 1) say("NOT VERY EFFECTIVE...", 0);
}

static void flash(void) {
  for (int i = 0; i < 3; i++) {
    rect(0, 0, W, H, WHITE); for (int k = 0; k < 4; k++) vsync();
    rect(0, 0, W, H, 0); for (int k = 0; k < 4; k++) vsync();
  }
}

// returns 1 = still alive (win/run/catch), 0 = whole party fainted
static int battle(void) {
  static Mon e;
  mk(&e, rnd() % 6, 2 + rnd() % 3 + wins / 3);
  int cur = firstalive();
  bp = &party[cur]; be = &e;
  flash();
  battle_draw();
  sset(L1, "A WILD ", sp[e.sp].name);
  say(L1, "APPEARED!");
  static const char *top[3] = { "FIGHT", "CATCH", "RUN" };
  for (;;) {
    battle_draw();
    int c = menu(3, top);
    int used = 1;
    if (c == 0) {
      const char *fm[3] = { "TACKLE", mv[sp[bp->sp].type], "BACK" };
      int m = menu(3, fm);
      if (m == 2) continue;
      attack(bp, be, m);
      if (be->hp <= 0) {
        battle_draw();
        sset(L1, "WILD ", sp[be->sp].name);
        say(L1, "FAINTED!");
        wins++;
        bp->xp++;
        if (bp->xp >= bp->lvl && bp->lvl < 30) {
          bp->lvl++; bp->xp = 0;
          int old = bp->maxhp;
          bp->maxhp = sp[bp->sp].hp + bp->lvl * 3;
          bp->hp += bp->maxhp - old;
          sset(L1, sp[bp->sp].name, " GREW!");
          sset(L2, "NOW LV ", ntoa(bp->lvl));
          say(L1, L2);
        }
        return 1;
      }
    } else if (c == 1) {
      if (pcount >= 4) { say("YOUR PARTY IS FULL!", 0); continue; }
      say("YOU THREW A CAPTURE CORE!", 0);
      int chance = 25 + 60 * (be->maxhp - be->hp) / be->maxhp;
      if ((int)(rnd() % 100) < chance) {
        party[pcount++] = *be;
        sset(L1, sp[be->sp].name, " JOINED");
        say(L1, "YOUR TEAM!");
        return 1;
      }
      say("IT BROKE FREE!", 0);
    } else {
      if (rnd() % 100 < 60) { say("GOT AWAY SAFELY!", 0); return 1; }
      say("COULDN'T ESCAPE!", 0);
    }
    if (used && be->hp > 0) {
      attack(be, bp, rnd() % 3 == 0);
      if (bp->hp <= 0) {
        battle_draw();
        sset(L1, sp[bp->sp].name, " FAINTED!");
        say(L1, 0);
        int n = firstalive();
        if (n < 0) return 0;
        cur = n; bp = &party[cur];
        battle_draw();
        sset(L1, "GO ", sp[bp->sp].name);
        say(L1, "!");
      }
    }
  }
}

/* ---------- overworld ---------- */
static const char *map[10] = {
  "TTTTTTTTTTTTTTT",
  "T,,,GGGG,,,~~~T",
  "T,,,GGGG,,,~~~T",
  "T,,,,GG,,,,,~~T",
  "T,TT,,,,,GGG,,T",
  "T,TT,,,,,GGG,,T",
  "T,,,,,GG,,,,,,T",
  "T,GGG,GG,,,TT,T",
  "T,GGG,,,,H,,,,T",
  "TTTTTTTTTTTTTTT",
};
static int px = 7, py = 4;

static void tile(int x, int y) {
  int X = x * 16, Y = y * 16;
  switch (map[y][x]) {
    case 'T':
      rect(X, Y, 16, 16, RGB(5, 17, 5));
      rect(X + 2, Y + 1, 12, 10, RGB(2, 10, 3));
      rect(X + 7, Y + 11, 3, 5, RGB(14, 8, 3));
      break;
    case 'G':
      rect(X, Y, 16, 16, RGB(10, 24, 8));
      for (int i = 0; i < 4; i++) rect(X + 2 + i * 4, Y + 3 + (i & 1) * 5, 2, 6, RGB(4, 16, 4));
      break;
    case '~':
      rect(X, Y, 16, 16, RGB(6, 14, 28));
      rect(X + 2, Y + 4, 6, 1, RGB(16, 24, 31));
      rect(X + 8, Y + 10, 6, 1, RGB(16, 24, 31));
      break;
    case 'H':
      rect(X, Y, 16, 16, RGB(22, 19, 12));
      rect(X + 1, Y + 1, 14, 6, RGB(28, 6, 6));
      rect(X + 2, Y + 7, 12, 8, WHITE);
      rect(X + 6, Y + 9, 4, 6, RGB(14, 8, 3));
      break;
    default:
      rect(X, Y, 16, 16, RGB(22, 19, 12));
  }
}
static void mapdraw(void) { for (int y = 0; y < 10; y++) for (int x = 0; x < 15; x++) tile(x, y); }
static void player(void) {
  int X = px * 16, Y = py * 16;
  rect(X + 4, Y + 1, 8, 6, RGB(31, 24, 18));
  rect(X + 4, Y + 1, 8, 2, RGB(10, 5, 2));
  rect(X + 3, Y + 7, 10, 8, RGB(6, 8, 28));
}

static void partyscreen(void) {
  rect(0, 0, W, H, RGB(3, 3, 10));
  text(10, 6, "YOUR TEAM", 2, WHITE);
  for (int i = 0; i < pcount; i++) {
    int y = 26 + i * 32;
    Mon *m = &party[i];
    drawmon(10, y, 3, sp[m->sp].col);
    text(46, y, sp[m->sp].name, 2, WHITE);
    text(46, y + 14, "LV", 1, WHITE); numat(62, y + 14, m->lvl, 1, WHITE);
    numat(100, y + 14, m->hp, 1, WHITE);
    text(120, y + 14, "/", 1, WHITE);
    numat(128, y + 14, m->maxhp, 1, WHITE);
    bar(150, y + 14, 70, m->hp, m->maxhp);
  }
  waitpress(K_A | K_B | K_START);
}

static void title(void) {
  rect(0, 0, W, H, RGB(2, 4, 14));
  for (int i = 0; i < 30; i++) rect(rnd() % W, rnd() % 100, 1, 1, WHITE);
  text(36, 40, "CREATURE QUEST", 3, RGB(31, 26, 6));
  drawmon(104, 76, 4, RGB(31, 12, 4));
  text(76, 126, "PRESS START", 2, WHITE);
  waitpress(K_START | K_A);
}

static void pick(void) {
  int sel = 0;
  for (;;) {
    rect(0, 0, W, H, RGB(4, 6, 16));
    text(44, 12, "CHOOSE YOUR PARTNER", 2, WHITE);
    for (int i = 0; i < 3; i++) {
      int x = 20 + i * 76;
      if (i == sel) rect(x - 6, 44, 44, 60, RGB(31, 26, 6));
      rect(x - 4, 46, 40, 56, RGB(4, 6, 16));
      drawmon(x, 54, 4, sp[i].col);
      text(x, 90, sp[i].name, 1, WHITE);
    }
    u16 p = waitpress(K_A | KEY_LEFT | KEY_RIGHT);
    if (p & K_A) break;
    if ((p & KEY_LEFT) && sel > 0) sel--;
    if ((p & KEY_RIGHT) && sel < 2) sel++;
  }
  mk(&party[0], sel, 3);
  pcount = 1;
  sset(L1, "YOU CHOSE ", sp[sel].name);
  say(L1, "GO EXPLORE!");
}

static void overworld(void) {
  int cd = 0;
  mapdraw(); player();
  for (;;) {
    vsync(); seed++;
    u16 k = keys();
    if (pressed() & K_START) { partyscreen(); mapdraw(); player(); continue; }
    if (cd > 0) { cd--; continue; }
    int dx = 0, dy = 0;
    if (k & KEY_UP) dy = -1;
    else if (k & KEY_DOWN) dy = 1;
    else if (k & KEY_LEFT) dx = -1;
    else if (k & KEY_RIGHT) dx = 1;
    if (!dx && !dy) continue;
    int nx = px + dx, ny = py + dy;
    char t = map[ny][nx];
    cd = 7;
    if (t == 'T' || t == '~') continue;
    tile(px, py);
    px = nx; py = ny;
    player();
    if (t == 'H') {
      healall();
      say("YOUR TEAM IS HEALED!", 0);
      mapdraw(); player();
    } else if (t == 'G' && rnd() % 7 == 0) {
      int ok = battle();
      if (!ok) {
        healall();
        say("YOU BLACKED OUT...", "TEAM WAS HEALED.");
        px = 7; py = 4;
      }
      mapdraw(); player();
      cd = 15;
    }
  }
}

int main(void) {
  REG_DISPCNT = 3 | (1 << 10);
  title();
  pick();
  overworld();
  return 0;
}
