/*
 * Minimale SSD1306 128x64 over I2C, deelt dezelfde i2c_bus als BME280.
 * Scrollend lijngrafiekje van temperatuur.
 */
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "ssd1306_graph.h"

#define SSD1306_ADDR        ((uint8_t)CONFIG_SSD1306_I2C_ADDR_7BIT)
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64
#define SSD1306_PAGES       (SSD1306_HEIGHT / 8)
#define FB_SIZE             (SSD1306_WIDTH * SSD1306_PAGES)

#define CTRL_CMD            0x00
#define CTRL_DATA           0x40

#define GRAPH_TOP           6
#define GRAPH_BOTTOM        61

#define GRAPH_LEFT          8
#define GRAPH_RIGHT         (SSD1306_WIDTH - 28)
#define GRAPH_WIDTH         (GRAPH_RIGHT - GRAPH_LEFT + 1)
#define LABEL_AREA_LEFT     0
#define LABEL_AREA_RIGHT    (GRAPH_LEFT - 1)

static const char *TAG = "ssd1306";

static i2c_bus_device_handle_t s_dev;
static uint8_t s_fb[FB_SIZE];
static float s_hist[SSD1306_WIDTH];
static bool s_have_hist;

static esp_err_t write_ctrl(const uint8_t *bytes, size_t len)
{
    if (len == 0 || len > 31) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buf[32];

    buf[0] = CTRL_CMD;
    memcpy(buf + 1, bytes, len);
    return i2c_bus_write_bytes(s_dev, NULL_I2C_MEM_ADDR, len + 1, buf);
}

static esp_err_t cmd1(uint8_t c)
{
    uint8_t b[2] = { CTRL_CMD, c };

    return i2c_bus_write_bytes(s_dev, NULL_I2C_MEM_ADDR, 2, b);
}

static esp_err_t flush_framebuffer_page_mode(void)
{
    esp_err_t err;

    err = write_ctrl((uint8_t[]){ 0x20, 0x10 }, 2);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t chunk[129];

    chunk[0] = CTRL_DATA;
    for (int page = 0; page < SSD1306_PAGES; page++) {
        err = cmd1(0xB0 | page);
        if (err != ESP_OK) {
            return err;
        }
        err = cmd1(0x00);
        if (err != ESP_OK) {
            return err;
        }
        err = cmd1(0x10);
        if (err != ESP_OK) {
            return err;
        }
        memcpy(chunk + 1, &s_fb[(size_t)page * SSD1306_WIDTH], 128);
        err = i2c_bus_write_bytes(s_dev, NULL_I2C_MEM_ADDR, 129, chunk);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static void fb_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
}

static void fb_set_pixel(int x, int y)
{
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return;
    }
    s_fb[x + (y / 8) * SSD1306_WIDTH] |= (uint8_t)(1u << (y & 7));
}

/* Eenvoudig 5x7 ASCII font voor 32..127 (we gebruiken alleen 0-9, minus en dot) */
static const uint8_t font5x7[][5] = {
    /* '0' (48) .. '9' (57) */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}  /* 9 */
};
/* minus '-' and dot '.' */
static const uint8_t font_minus[5] = {0x08,0x08,0x08,0x08,0x08};
static const uint8_t font_dot[5]   = {0x00,0x60,0x60,0x00,0x00};

static void fb_draw_char_small(int x, int y, char c)
{
    const uint8_t *glyph = NULL;
    if (c >= '0' && c <= '9') {
        glyph = font5x7[c - '0'];
    } else if (c == '-') {
        glyph = font_minus;
    } else if (c == '.') {
        glyph = font_dot;
    } else {
        return; /* niet ondersteund */
    }

    for (int col = 0; col < 5; col++) {
        uint8_t colbits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (colbits & (1 << row)) {
                fb_set_pixel(x + col, y + row);
            }
        }
    }
}

static void fb_draw_text_small(int x, int y, const char *s)
{
    while (*s) {
        fb_draw_char_small(x, y, *s++);
        x += 6; /* 5px + 1px spacing */
    }
}


static void draw_hline(int x0, int x1, int y)
{
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int x = x0; x <= x1; x++) {
        fb_set_pixel(x, y);
    }
}

static void draw_line(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        fb_set_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static int map_temp_y(float t, float tmin, float tmax)
{
    if (tmax <= tmin) {
        tmax = tmin + 0.1f;
    }
    float f = (t - tmin) / (tmax - tmin);
    if (f < 0.f) {
        f = 0.f;
    }
    if (f > 1.f) {
        f = 1.f;
    }
    int y = GRAPH_BOTTOM - (int)(f * (float)(GRAPH_BOTTOM - GRAPH_TOP) + 0.5f);
    if (y < GRAPH_TOP) {
        y = GRAPH_TOP;
    }
    if (y > GRAPH_BOTTOM) {
        y = GRAPH_BOTTOM;
    }
    return y;
}

static void redraw_graph(float latest)
{
    /* schuif historie en voeg nieuwe waarde toe */
    memmove(s_hist, s_hist + 1, sizeof(s_hist[0]) * (SSD1306_WIDTH - 1));
    s_hist[SSD1306_WIDTH - 1] = latest;
    s_have_hist = true;

    fb_clear();

    /* teken boven/onder rand van grafiek */
    draw_hline(GRAPH_LEFT, GRAPH_RIGHT, GRAPH_TOP - 1);
    draw_hline(GRAPH_LEFT, GRAPH_RIGHT, GRAPH_BOTTOM + 1);

    /* bepaal min/max over de zichtbare grafiekbreedte (laatste samples) */
    float tmin = latest;
    float tmax = latest;
    int start_idx = SSD1306_WIDTH - GRAPH_WIDTH;
    if (start_idx < 0) start_idx = 0;
    for (int i = start_idx; i < SSD1306_WIDTH; i++) {
        float v = s_hist[i];
        if (v < tmin) tmin = v;
        if (v > tmax) tmax = v;
    }
    float margin = 0.5f;
    tmin -= margin;
    tmax += margin;
    if (tmax - tmin < 1.0f) tmax = tmin + 1.0f;

    /* teken horizontale gridlijnen en Y-labels (min, mid, max) */
    float mid = (tmin + tmax) / 2.0f;
    int y_min = map_temp_y(tmin, tmin, tmax);
    int y_mid = map_temp_y(mid, tmin, tmax);
    int y_max = map_temp_y(tmax, tmin, tmax);

    /* ticks op linker rand (blijven exact op de y-positie) */
    draw_hline(GRAPH_LEFT - 2, GRAPH_LEFT - 1, y_min);
    draw_hline(GRAPH_LEFT - 2, GRAPH_LEFT - 1, y_mid);
    draw_hline(GRAPH_LEFT - 2, GRAPH_LEFT - 1, y_max);

    /* labels (tekst) links — clamp zodat ze niet over de boven/onderlijn tekenen */
    const int text_h = 7;      /* fonthoogte 7px */
    const int pad = 3;         /* minimale afstand tot rand */
    const int y_text_min = GRAPH_TOP + pad;
    const int y_text_max = GRAPH_BOTTOM - text_h - pad;

    int ly_max = y_max - (text_h / 2);
    if (ly_max < y_text_min) ly_max = y_text_min;
    if (ly_max > y_text_max) ly_max = y_text_max;

    int ly_mid = y_mid - (text_h / 2);
    if (ly_mid < y_text_min) ly_mid = y_text_min;
    if (ly_mid > y_text_max) ly_mid = y_text_max;

    int ly_min = y_min - (text_h / 2);
    if (ly_min < y_text_min) ly_min = y_text_min;
    if (ly_min > y_text_max) ly_min = y_text_max;

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", tmax);
    fb_draw_text_small(0, ly_max, buf);
    snprintf(buf, sizeof(buf), "%.1f", mid);
    fb_draw_text_small(0, ly_mid, buf);
    snprintf(buf, sizeof(buf), "%.1f", tmin);
    fb_draw_text_small(0, ly_min, buf);

    /* teken de grafieklijn en punten; we mappen historie naar GRAPH_LEFT..GRAPH_RIGHT */
    for (int x = 1; x < GRAPH_WIDTH; x++) {
        int idx0 = x - 1;
        int idx1 = x;
        float v0 = s_hist[start_idx + idx0];
        float v1 = s_hist[start_idx + idx1];
        int y0 = map_temp_y(v0, tmin, tmax);
        int y1 = map_temp_y(v1, tmin, tmax);
        draw_line(GRAPH_LEFT + idx0, y0, GRAPH_LEFT + idx1, y1);
        /* teken punt op sample (zichtbaarer maken door 1 extra pixel) */
        fb_set_pixel(GRAPH_LEFT + idx1, y1);
        if (y1 > 0) fb_set_pixel(GRAPH_LEFT + idx1, y1 - 1);
    }

    /* verticale balk rechts als indicator van actuele waarde */
    int ybar = map_temp_y(latest, tmin, tmax);
    for (int yy = GRAPH_BOTTOM; yy >= ybar; yy--) {
        fb_set_pixel(GRAPH_RIGHT - 1, yy);
        fb_set_pixel(GRAPH_RIGHT, yy);
    }

    /* actuele waarde als tekst rechtsboven in labelgebied (clamp y indien nodig) */
    snprintf(buf, sizeof(buf), "%.1fC", latest);
    int tx = GRAPH_RIGHT + 2;
    int ty = GRAPH_TOP;
    if (ty < 0) ty = 0;
    if (ty > SSD1306_HEIGHT - text_h) ty = SSD1306_HEIGHT - text_h;
    fb_draw_text_small(tx, ty, buf);
}



esp_err_t ssd1306_graph_init(i2c_bus_handle_t bus)
{
    esp_err_t err;

    ssd1306_graph_deinit();

    s_dev = i2c_bus_device_create(bus, SSD1306_ADDR, 0);
    if (s_dev == NULL) {
        ESP_LOGW(TAG, "Geen I2C-device op 0x%02X (SSD1306?)", SSD1306_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    err = write_ctrl((uint8_t[]){ 0xAE }, 1);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0xD5, 0x80 }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0xA8, 0x3F }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0xD3, 0x00 }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = cmd1(0x40);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0x8D, 0x14 }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0x20, 0x10 }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = cmd1(0xA1);
    if (err != ESP_OK) {
        goto fail;
    }
    err = cmd1(0xC8);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0xDA, 0x12 }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0x81, 0x7F }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0xD9, 0x22 }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = write_ctrl((uint8_t[]){ 0xDB, 0x20 }, 2);
    if (err != ESP_OK) {
        goto fail;
    }
    err = cmd1(0xA4);
    if (err != ESP_OK) {
        goto fail;
    }
    err = cmd1(0xA6);
    if (err != ESP_OK) {
        goto fail;
    }
    err = cmd1(0xAF);
    if (err != ESP_OK) {
        goto fail;
    }

    memset(s_hist, 0, sizeof(s_hist));
    s_have_hist = false;
    fb_clear();
    err = flush_framebuffer_page_mode();
    if (err != ESP_OK) {
        goto fail;
    }

    ESP_LOGI(TAG, "SSD1306 actief op I2C 0x%02X (grafiek %dx%d)", SSD1306_ADDR, SSD1306_WIDTH, SSD1306_HEIGHT);
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "SSD1306 init mislukt: %s", esp_err_to_name(err));
    i2c_bus_device_delete(&s_dev);
    return err;
}

void ssd1306_graph_deinit(void)
{
    if (s_dev != NULL) {
        i2c_bus_device_delete(&s_dev);
        s_dev = NULL;
    }
    s_have_hist = false;
}

void ssd1306_graph_push_temperature(float temp_c)
{
    if (s_dev == NULL) {
        return;
    }

    if (!s_have_hist) {
        for (int i = 0; i < SSD1306_WIDTH; i++) {
            s_hist[i] = temp_c;
        }
        s_have_hist = true;
    }

    redraw_graph(temp_c);
    esp_err_t e = flush_framebuffer_page_mode();
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "Display update: %s", esp_err_to_name(e));
    }
}

bool ssd1306_graph_is_active(void)
{
    return s_dev != NULL;
}
