#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <vector>
#include <map>
#include "pico/time.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "drivers/st7735.h"
#include <hardware/clocks.h>
#include "images.h"

#define PIN_LEFT   10
#define PIN_RIGHT  8
#define PIN_ROT    13
#define PIN_SDROP  12
#define PIN_HDROP  17
#define PIN_PAUSE  16
#define PIN_ROT_CCW  11
#define PIN_HOLD 15

#define DAS_MS 142
#define ARR_MS 1

#define PIECE_COLOR ST7735_WHITE
#define FIELD_COLOR ST7735_WHITE


static void btn_init(int pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

std::map<int, uint16_t> number_to_color = {
    {0, 0x5ffe},
    {1, 0xD5a9},
    {2, 0xaa54},
    {3, 0x85a6},
    {4, 0xfa08},
    {5, 0x4a7f},
    {6, 0xfe67},
};

class ButtonHandler {
public:
    ButtonHandler(uint pin, void (*callback)())
        : pin(pin), callback(callback), state(false),
          pressedTime(0), lastRepeatTime(0), repeating(false) 
    {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }

    void update() {
        bool isPressed = readButton();
        uint64_t now = millis();

        if (isPressed && !state) {
            // Button just pressed
            state = true;
            pressedTime = now;
            lastRepeatTime = now;
            repeating = false;
            callback();
        }
        else if (isPressed && state) {
            // Button is being held
            if (!repeating && (now - pressedTime >= DAS_MS)) {
                repeating = true;
                lastRepeatTime = now;
                callback();
            }
            else if (repeating && (now - lastRepeatTime >= ARR_MS)) {
                lastRepeatTime = now;
                callback(); // Subsequent repeats
            }
        }
        else if (!isPressed && state) {
            // Button released
            state = false;
            repeating = false;
        }
    }

private:
    uint pin;
    bool state;
    bool repeating;
    uint64_t pressedTime;
    uint64_t lastRepeatTime;
    void (*callback)();

    uint64_t millis() {
        return time_us_64() / 1000;
    }

    bool readButton() {
        return gpio_get(pin) == 0;
    }
};

uint32_t fps_counter = 0;
uint32_t fps_value = 0;
absolute_time_t fps_timer;

void update_fps() {
    fps_counter++;
    if (absolute_time_diff_us(fps_timer, get_absolute_time()) / 1000000 >= 1) {
        fps_value = fps_counter;
        fps_counter = 0;
        fps_timer = get_absolute_time();
    }
}

#define ghost_lines 1
static const int CELL_W = 6;   // px
static const int CELL_H = 6;   // px
static const int COLS   = 10;
static const int ROWS   = 20 + ghost_lines;

static const int FIELD_W = COLS * CELL_W;
static const int FIELD_H = (ROWS - ghost_lines) * CELL_H;

static const int FIELD_X = 2;                        // left margin
static const int FIELD_Y = (128 - FIELD_H-10)/2;        // vertical offset
static const int PANEL_X = FIELD_X + FIELD_W + 4;    // 56
static const int PANEL_W = 64 - PANEL_X - 2;         // ~6
static const int PANEL_Y = FIELD_Y;
static const int PANEL_H = FIELD_H;

static const int MINI = 4;
static const int PREV_BOX_W = 4*MINI + 2;
static const int PREV_BOX_H = 4*MINI + 2;
static const int PREV_SPACING = 4;         // vertical spacing between previews
static const int NEXT_SHOW = 4;            // show 4 next pieces

static const int SOFT_DROP_MS  = 1;   // soft drop tick
static const int LOCK_DELAY_MS = 500;  // lock delay when grounded

static uint8_t board[ROWS][COLS];

static const uint8_t TETROMINOES[7][4][16] = {
    // I
    {
        {0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0},
        {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0},
        {0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0},
        {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0}
    },
    // O
    {
        {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
        {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
        {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
        {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0}
    },
    // T
    {
        {0,1,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
        {0,1,0,0, 0,1,1,0, 0,1,0,0, 0,0,0,0},
        {0,0,0,0, 1,1,1,0, 0,1,0,0, 0,0,0,0},
        {0,1,0,0, 1,1,0,0, 0,1,0,0, 0,0,0,0}
    },
    // S
    {
        {0,1,1,0, 1,1,0,0, 0,0,0,0, 0,0,0,0},
        {0,1,0,0, 0,1,1,0, 0,0,1,0, 0,0,0,0},
        {0,0,0,0, 0,1,1,0, 1,1,0,0, 0,0,0,0},
        {1,0,0,0, 1,1,0,0, 0,1,0,0, 0,0,0,0}
    },
    // Z
    {
        {1,1,0,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
        {0,0,1,0, 0,1,1,0, 0,1,0,0, 0,0,0,0},
        {0,0,0,0, 1,1,0,0, 0,1,1,0, 0,0,0,0},
        {0,1,0,0, 1,1,0,0, 1,0,0,0, 0,0,0,0}
    },
    // J
    {
        {1,0,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
        {0,1,1,0, 0,1,0,0, 0,1,0,0, 0,0,0,0},
        {0,0,0,0, 1,1,1,0, 0,0,1,0, 0,0,0,0},
        {0,1,0,0, 0,1,0,0, 1,1,0,0, 0,0,0,0}
    },
    // L
    {
        {0,0,1,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
        {0,1,0,0, 0,1,0,0, 0,1,1,0, 0,0,0,0},
        {0,0,0,0, 1,1,1,0, 1,0,0,0, 0,0,0,0},
        {1,1,0,0, 0,1,0,0, 0,1,0,0, 0,0,0,0}
    }
};

enum PieceType {O=1,T=2,S=3,Z=4,J=5,L=6,I=7};

struct Piece {
    int t; // 1..7
    int r; // 0..3
    int x; // col
    int y; // row
};

static Piece cur;
static Piece holded;
static std::vector<int> bag;
static std::vector<int> next_queue;
static bool paused = false;
static int score = 0;
static int lines_cleared = 0;
static int level = 1;

// RNG & 7-bag 
static uint32_t rng_state = 0x12345678;
static inline uint32_t xorshift32() {
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x; return x;
}
static void shuffle7(int a[7]) {
    for (int i=6;i>0;i--) {
        int j = xorshift32() % (i+1);
        int tmp=a[i]; a[i]=a[j]; a[j]=tmp;
    }
}
static void refill_bag() {
    int a[7] = {0,1,2,3,4,5,6};
    shuffle7(a);
    for (int i=0;i<7;i++) bag.push_back(a[i]);
}
static int get_next_piece() {
    if (bag.empty()) refill_bag();
    int t = bag.front(); bag.erase(bag.begin());
    return t;
}
static void ensure_next_queue(int n) {
    while ((int)next_queue.size() < n) {
        next_queue.push_back(get_next_piece());
    }
}

// wall kicks
static const int8_t SRS_JLSTZ[4][5][2] = {
    // 0->1
    { { 0, 0}, {-1, 0}, {-1, -1}, { 0, 2}, {-1, 2} },
    // 1->2
    { { 0, 0}, {+1, 0}, {+1,+1}, { 0,-2}, {+1,-2} },
    // 2->3
    { { 0, 0}, {+1, 0}, {+1,-1}, { 0,+2}, {+1,+2} },
    // 3->0
    { { 0, 0}, {-1, 0}, {-1,+1}, { 0,-2}, {-1,-2} }
};

// static const int8_t SRS_JLSTZ[4][5][2] = {
//     // 0->1
//     { { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5} },
//     // 1->2
//     { { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5} },
//     // 2->3
//     { { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5} },
//     // 3->0
//     { { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5}, { 0, -5} },
// };

static const int8_t SRS_I[4][5][2] = {
    // 0->1
    { { 0, 0}, {-2, 0}, {+1, 0}, {-2,+1}, {+1,-2} },
    // 1->2
    { { 0, 0}, {-1, 0}, {+2, 0}, {-1,-2}, {+2,+1} },
    // 2->3
    { { 0, 0}, {-2, 0}, {-1, 0}, {+2,-1}, {-1,+2} },
    // 3->0
    { { 0, 0}, {+1, 0}, {-2, 0}, {+1,+2}, {-2,-1} }
};

static const int8_t SRS_O[4][1][2] = {
    { {0,0} }, { {0,0} }, { {0,0} }, { {0,0} }
};

static inline bool in_bounds(int r, int c) {
    return (r>=0 && r<ROWS && c>=0 && c<COLS);
}
static inline bool cell_blocked(int r, int c) {
    if (!in_bounds(r,c)) return true;
    return board[r][c] != 0;
}
static bool can_place(const Piece& p) {
    const uint8_t* sh = TETROMINOES[p.t][p.r];
    for (int yy = 0; yy < 4; ++yy) {
        for (int xx = 0; xx < 4; ++xx) {
            if (!sh[yy*4 + xx]) continue;

            int r = p.y + yy;
            int c = p.x + xx;

            if (r < 0) {
                if (c < 0 || c >= COLS) return false;
                continue;
            }

            if (c < 0 || c >= COLS || r >= ROWS) return false;
            if (board[r][c]) return false;
        }
    }
    return true;
}

static inline bool btn_down(int pin) {
    return gpio_get(pin) == 0;
}

static bool try_rotate_srs(Piece &pc, int dir) {
    int from = pc.r;
    int to   = (pc.r + (dir>0?1:3)) & 3;
    Piece test = pc; test.r = to;

    if (pc.t == O) {
        return true;
    }
    const int8_t (*tab)[5][2] = (pc.t == I) ? SRS_I : SRS_JLSTZ;
    int idx = from;
    for (int i=0;i<5;i++) {
        int dx = tab[idx][i][0], dy = tab[idx][i][1];
        test.x = pc.x + dx; test.y = pc.y + dy;
        if (can_place(test)) { pc = test; return true; }
    }
    return false;
}

static bool grounded(const Piece& p) {
    Piece t = p; t.y++;
    return !can_place(t);
}

static void lock_piece(const Piece& p) {
    const uint8_t* sh = TETROMINOES[p.t][p.r];
    for (int yy=0; yy<4; yy++)
        for (int xx=0; xx<4; xx++)
            if (sh[yy*4 + xx]) {
                int r = p.y + yy;
                int c = p.x + xx;
                if (in_bounds(r,c)) board[r][c] = p.t + 1;
            }
}

static int clear_lines() {
    int cleared = 0;
    for (int r = ROWS-1; r >= 0; r--) {
        bool full = true;
        for (int c=0;c<COLS;c++) if (!board[r][c]) { full=false; break; }
        if (full) {
            cleared++;
            for (int rr=r; rr>0; rr--)
                for (int c=0;c<COLS;c++)
                    board[rr][c] = board[rr-1][c];
            for (int c=0;c<COLS;c++) board[0][c] = 0;
            r++;
        }
    }
    return cleared;
}
bool was_holded_this_turn = false;

static void new_piece_from_queue() {
    was_holded_this_turn = false;
    ensure_next_queue(7);
    cur.t = next_queue.front();
    next_queue.erase(next_queue.begin());
    cur.r = 0;
    cur.x = (COLS/2) - 2;
    cur.y = 0;
    ensure_next_queue(7);
    // game over check
    for (uint8_t i = 0; i < COLS; i++){
        if (board[0][i]) {
            memset(board, 0, sizeof(board));
            score = 0; lines_cleared = 0; level = 1;
            next_queue.clear(); ensure_next_queue(7);
            ST7735_FillScreen(ST7735_BLACK);
            break;
        }
    }
}

void move_left() {
    Piece t = cur; t.x--;
    if (can_place(t)) {
        cur = t;
    }
}

void move_right() {
    Piece t = cur; t.x++;
    if (can_place(t)) {
        cur = t;
    }
}

static Piece ghost_of(const Piece& p) {
    Piece g = p;
    while (true) {
        Piece n = g; n.y++;
        if (can_place(n)) g = n; else break;
    }
    return g;
}

static void draw_holded_cell(int c, int r, int piece_type) {
    int x = FIELD_X + c * CELL_W;
    int y = FIELD_Y + r * CELL_H;
    int w = CELL_W - 1;
    int h = CELL_H - 1;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (was_holded_this_turn)ST7735_DrawRectFill(x, y, w + 1, h + 1, 0x73ae);
    else ST7735_DrawRectFill(x, y, w + 1, h + 1, number_to_color[piece_type]);
}

static void draw_cell(int c, int r, int piece_type) {
    int x = FIELD_X + c * CELL_W;
    int y = FIELD_Y + r * CELL_H;
    int w = CELL_W - 1;
    int h = CELL_H - 1;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    ST7735_DrawRectFill(x, y, w + 1, h + 1, number_to_color[piece_type]);
}

static void draw_cell_ghost(int c, int r) {
    int x = FIELD_X + c * CELL_W;
    int y = FIELD_Y + r * CELL_H;
    int w = CELL_W - 1; if (w < 1) w = 1;
    int h = CELL_H - 1; if (h < 1) h = 1;

    for (int xx=0; xx<w; xx++) {
        ST7735_DrawPixel(x + xx, y, PIECE_COLOR);
        ST7735_DrawPixel(x + xx, y+h-1, PIECE_COLOR);
    }
    for (int yy=0; yy<h; yy++) {
        ST7735_DrawPixel(x, y + yy, PIECE_COLOR);
        ST7735_DrawPixel(x + w - 1, y+yy, PIECE_COLOR);
    }

}

static void draw_field_outline() {
    ST7735_DrawRectFill(FIELD_X-1, FIELD_Y-1 + ghost_lines * CELL_W, FIELD_W+2, 1, FIELD_COLOR);
    ST7735_DrawRectFill(FIELD_X-1, FIELD_Y+FIELD_H + ghost_lines * CELL_W, FIELD_W+2, 1, FIELD_COLOR);
    ST7735_DrawRectFill(FIELD_X-1, FIELD_Y+ ghost_lines * CELL_W, 1, FIELD_H, FIELD_COLOR);
    ST7735_DrawRectFill(FIELD_X+FIELD_W, FIELD_Y+ ghost_lines * CELL_W, 1, FIELD_H, FIELD_COLOR);
}

static void draw_board() {
    for (int r=0;r<ROWS;r++)
        for (int c=0;c<COLS;c++)
            if (board[r][c]) draw_cell(c, r, board[r][c]-1);
    }

static void draw_piece(const Piece& p) {
    const uint8_t* sh = TETROMINOES[p.t][p.r];
    for (int yy=0; yy<4; yy++)
        for (int xx=0; xx<4; xx++)
            if (sh[yy*4 + xx]) {
                int r = p.y + yy;
                int c = p.x + xx;
                if (r>=0) draw_cell(c, r, p.t); 
            }
}

static void draw_holded(const Piece& p) {
    if (p.t == -1) {return;}
    const uint8_t* sh = TETROMINOES[p.t][0]; // 0 rotation
    for (int yy=0; yy<4; yy++)
        for (int xx=0; xx<4; xx++)
            if (sh[yy*4 + xx]) {
                int r = 3 + yy; // out of board coordinates
                int c = 16 + xx; // out of board coordinates
                if (r>=0) draw_holded_cell(c, r, p.t); 
            }
}


static void draw_ghost(const Piece& p) {
    Piece g = ghost_of(p);
    const uint8_t* sh = TETROMINOES[g.t][g.r];
    for (int yy=0; yy<4; yy++)
        for (int xx=0; xx<4; xx++)
            if (sh[yy*4 + xx]) {
                int r = g.y + yy;
                int c = g.x + xx;
                if (r>=0) draw_cell_ghost(c,r);
            }
}

static void draw_mini_piece(int t, int x, int y) {
    const uint8_t* sh = TETROMINOES[t][0];
    for (int yy=0; yy<4; yy++)
        for (int xx=0; xx<4; xx++)
            if (sh[yy*4 + xx]) {
                int px = x + 1 + xx*MINI;
                int py = y + 1 + yy*MINI;
                ST7735_DrawRectFill(px, py, MINI, MINI, number_to_color[t]);
            }
}

static void draw_queue() {
    int y = PANEL_Y + 2 + ghost_lines * CELL_W;
    for (int i=0; i<NEXT_SHOW && i<(int)next_queue.size(); i++) {
        draw_mini_piece(next_queue[i], PANEL_X, y);
        y += PREV_BOX_H + PREV_SPACING;
        if (y > PANEL_Y + PANEL_H - PREV_BOX_H) break;
    }
}

static absolute_time_t last_fall;
static absolute_time_t last_softdrop;
static bool was_grounded = false;
static absolute_time_t grounded_time;

static int gravity_interval_ms() {
    if (level < 2)  return 700;
    if (level < 4)  return 500;
    if (level < 6)  return 380;
    if (level < 8)  return 300;
    if (level < 10) return 220;
    if (level < 12) return 160;
    if (level < 15) return 120;
    if (level < 18) return 90;
    return 70;
}

static void move_lr(int dir){
    Piece t = cur; t.x+=dir;
    if(can_place(t)) cur = t;
}

static void on_piece_locked() {
    lock_piece(cur);
    int cl = clear_lines();
    if (cl) {
        lines_cleared += cl;
        static const int pts[5] = {0,40,100,300,1200};
        score += pts[cl] * level;
        level = 1 + lines_cleared / 10;
    }
    new_piece_from_queue();
    was_grounded = false;
}

static void tick_fall() {
    Piece t = cur; t.y++;
    if (can_place(t)) {
        cur = t;
        was_grounded = false;
        score++;
    } else {
        if (!was_grounded) {
            was_grounded = true;
            grounded_time = get_absolute_time();
        } else {
            if (absolute_time_diff_us(grounded_time, get_absolute_time()) >= (int64_t)LOCK_DELAY_MS*1000) {
                on_piece_locked();
            }
        }
    }
}

static void reset_lock_if_grounded_changed(bool moved_or_rotated) {
    if (moved_or_rotated && grounded(cur)) {
        grounded_time = get_absolute_time();
        was_grounded = true;
    }
}

int main() {
    // set_sys_clock_khz(100000, true);
    holded.t = -1;
    stdio_init_all();

    ST7735_Init();
    ST7735_FillScreen(ST7735_BLACK);
    btn_init(PIN_ROT);
    btn_init(PIN_ROT_CCW);
    btn_init(PIN_SDROP);
    btn_init(PIN_HDROP);
    btn_init(PIN_PAUSE);
    btn_init(PIN_HOLD);
    
    ButtonHandler btn_L(PIN_LEFT, move_left);
    ButtonHandler btn_R(PIN_RIGHT, move_right);

    rng_state ^= (uint32_t)time_us_64();

    memset(board, 0, sizeof(board));
    bag.clear(); next_queue.clear();
    ensure_next_queue(7);
    new_piece_from_queue();

    last_fall = get_absolute_time();
    last_softdrop = get_absolute_time();

    while (true) {
        ST7735_FillScreen(ST7735_BLACK);

        char level_string[20];
        snprintf(level_string, sizeof(level_string), "%d", level);
        
        char fps_string[20];
        snprintf(fps_string, sizeof(fps_string), "%d", fps_value);

        ST7735_DrawString(100, 45, level_string, Font_11x18, ST7735_WHITE);
        ST7735_DrawString(93, 68, fps_string, Font_11x18, ST7735_GREEN);

        update_fps();
        static bool prev_pause = false;
        bool p_pause = btn_down(PIN_PAUSE);
        if (p_pause && !prev_pause) paused = !paused;
        prev_pause = p_pause;

        if (!paused) {
            btn_R.update();
            btn_L.update();
            static bool prev_rot = false;

            bool rot = btn_down(PIN_ROT);
            bool rot_ccw = btn_down(PIN_ROT_CCW);
            if ((rot_ccw | rot) && !prev_rot) {
                Piece before = cur;
                int dir;
                if (rot)dir = +1;
                else dir = -1;
                if (try_rotate_srs(cur, dir)) {
                    reset_lock_if_grounded_changed(true);
                } else {
                    cur = before;
                }
            }

            if (rot) prev_rot = rot;
            else prev_rot = rot_ccw;

            bool press_hold = btn_down(PIN_HOLD);
            
            if (press_hold && !was_holded_this_turn) {
                was_holded_this_turn = true;

                if (holded.t == -1) {
                    holded = cur;
                    new_piece_from_queue();
                }
                else {
                    Piece swap = cur;
                    cur = holded;
                    cur.x = (COLS/2) - 2;
                    cur.y = 0;
                    cur.r = 0;
                    holded = swap;
                    }
            }

            bool sdrop = btn_down(PIN_SDROP);
            if (sdrop) {
                if (absolute_time_diff_us(last_softdrop, get_absolute_time()) >= (int64_t)SOFT_DROP_MS*10) {
                    last_softdrop = get_absolute_time();
                    Piece t = cur; t.y++;
                    if (can_place(t)) {
                        cur = t;
                        was_grounded = false;
                        score += 1;
                    } else {
                        if (!was_grounded) {
                            was_grounded = true;
                            grounded_time = get_absolute_time();
                        } //else if (absolute_time_diff_us(grounded_time, get_absolute_time()) >= (int64_t)LOCK_DELAY_MS*10) {
                        //     on_piece_locked();
                        // }
                    }
                }
            }

            static bool prev_hard = false;
            bool hdrop = btn_down(PIN_HDROP);
            if (hdrop && !prev_hard) {
                Piece g = ghost_of(cur);
                int dy = g.y - cur.y;
                if (dy > 0) score += 2*dy;
                cur = g;
                on_piece_locked();
            }
            prev_hard = hdrop;

            int fall_ms = gravity_interval_ms();
            if (absolute_time_diff_us(last_fall, get_absolute_time()) >= (int64_t)fall_ms * 1000) {
                last_fall = get_absolute_time();
                tick_fall();
            }
        }
        
        // Render
        // ST7735_DrawImage(0, 0, 160, 128, cat_farmer);
        draw_field_outline();
        draw_board();
        draw_ghost(cur);
        draw_piece(cur);
        draw_holded(holded);
        draw_queue();

        if (paused) { //pause case
            ST7735_DrawRectFill(80-10, 64-16, 5, 20, ST7735_BLACK);
            ST7735_DrawRectFill(80, 64-16, 5, 20, ST7735_BLACK);

            ST7735_DrawRect(79-10, 63-16, 7, 22, ST7735_WHITE);
            ST7735_DrawRect(79, 63-16, 7, 22, ST7735_WHITE);
        }
        ST7735_Update();

    }

    return 0;
}

