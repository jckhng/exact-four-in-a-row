#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <stdio.h>
#include <string.h>

#include "gnect_engine.h"

#define APP_TITLE "Kindle Gnect"
#define KINDLE_WINDOW_TITLE "L:A_N:application_ID:kindlegnect_PC:N_O:URL"
#define SAVE_PATH "/mnt/us/documents/kindle-gnect.txt"
#define LOG_PATH "/mnt/us/kindle-gnect.log"
#define KINDLE_APP_WIDTH 1072
#define KINDLE_APP_HEIGHT 1448

typedef enum {
    MODE_PLAY_RED = 0,
    MODE_PLAY_YELLOW = 1,
    MODE_TWO_PLAYER = 2,
    MODE_AI_DEMO = 3
} AppMode;

typedef struct {
    GtkWidget *window;
    GtkWidget *board;
    GtkWidget *status;
    GtkWidget *red_score;
    GtkWidget *yellow_score;
    GtkWidget *moves_label;
    GtkWidget *mode_combo;
    GtkWidget *level_combo;
    GtkWidget *history_first_button;
    GtkWidget *history_prev_button;
    GtkWidget *history_next_button;
    GtkWidget *history_latest_button;
    GnectGame game;
    AppMode mode;
    int level;
    int view_ply;
    guint ai_source;
    char message[160];
} AppState;

static AppState app;

static gboolean is_ai_turn(void);
static void update_ui(void);
static void new_game(void);

static void app_log(const char *message)
{
    FILE *f = fopen(LOG_PATH, "a");
    if (!f)
        return;
    fprintf(f, "[app] %s\n", message);
    fclose(f);
}

static void app_apply_high_contrast(GtkWidget *widget)
{
    GdkColor black = {0, 0x0000, 0x0000, 0x0000};
    GdkColor white = {0, 0xffff, 0xffff, 0xffff};

    gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_ACTIVE, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_text(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_text(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_base(widget, GTK_STATE_SELECTED, &black);
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_bg(widget, GTK_STATE_SELECTED, &black);
}

static void app_install_kindle_style(void)
{
    gtk_rc_parse_string(
        "style \"kindle_high_contrast\" {\n"
        "  fg[NORMAL] = \"#000000\"\n"
        "  fg[ACTIVE] = \"#000000\"\n"
        "  fg[PRELIGHT] = \"#ffffff\"\n"
        "  fg[SELECTED] = \"#ffffff\"\n"
        "  text[NORMAL] = \"#000000\"\n"
        "  text[ACTIVE] = \"#000000\"\n"
        "  text[SELECTED] = \"#ffffff\"\n"
        "  base[NORMAL] = \"#ffffff\"\n"
        "  base[ACTIVE] = \"#ffffff\"\n"
        "  base[SELECTED] = \"#000000\"\n"
        "  bg[NORMAL] = \"#ffffff\"\n"
        "  bg[ACTIVE] = \"#ffffff\"\n"
        "  bg[PRELIGHT] = \"#000000\"\n"
        "  bg[SELECTED] = \"#000000\"\n"
        "}\n"
        "gtk-button-images = 0\n"
        "gtk-menu-images = 0\n"
        "class \"GtkComboBox\" style \"kindle_high_contrast\"\n"
        "class \"GtkCellView\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenu\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenuItem\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkComboBox*\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkMenu*\" style \"kindle_high_contrast\"\n");
}

static const char *piece_name(GnectPiece piece)
{
    if (piece == GNECT_RED)
        return "Red";
    if (piece == GNECT_YELLOW)
        return "Yellow";
    return "None";
}

static int current_view_ply(void)
{
    if (app.view_ply < 0 || app.view_ply > app.game.move_count)
        return app.game.move_count;
    return app.view_ply;
}

static gboolean is_viewing_latest(void)
{
    return current_view_ply() == app.game.move_count;
}

static GnectPiece viewed_piece_at(int col, int row)
{
    return app.game.history[current_view_ply()][row][col];
}

static gboolean is_ai_turn(void)
{
    if (app.game.winner != GNECT_EMPTY || !is_viewing_latest())
        return FALSE;

    if (app.mode == MODE_TWO_PLAYER)
        return FALSE;
    if (app.mode == MODE_AI_DEMO)
        return TRUE;
    if (app.mode == MODE_PLAY_RED)
        return app.game.turn == GNECT_YELLOW;
    return app.game.turn == GNECT_RED;
}

static void set_message(const char *message)
{
    g_strlcpy(app.message, message, sizeof(app.message));
}

static void draw_centered_text(cairo_t *cr, const char *text, double cx, double cy, double size)
{
    cairo_text_extents_t extents;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0 - extents.x_bearing,
                  cy - extents.height / 2.0 - extents.y_bearing);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

static void draw_piece(cairo_t *cr, GnectPiece piece, double cx, double cy, double radius)
{
    double x;

    cairo_save(cr);
    cairo_new_path(cr);
    cairo_arc(cr, cx + radius * 0.08, cy + radius * 0.10, radius, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.22);
    cairo_fill(cr);

    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, radius, 0, 2 * G_PI);
    if (piece == GNECT_RED)
        cairo_set_source_rgb(cr, 0.04, 0.04, 0.04);
    else
        cairo_set_source_rgb(cr, 0.96, 0.96, 0.90);
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, MAX(3.0, radius * 0.10));
    cairo_stroke(cr);

    if (piece == GNECT_YELLOW) {
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, radius * 0.82, 0, 2 * G_PI);
        cairo_clip(cr);
        cairo_new_path(cr);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_line_width(cr, MAX(1.4, radius * 0.045));
        for (x = cx - radius * 1.2; x < cx + radius * 1.2; x += radius * 0.26) {
            cairo_move_to(cr, x, cy + radius);
            cairo_line_to(cr, x + radius, cy - radius);
        }
        cairo_stroke(cr);
    }

    cairo_restore(cr);

    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, radius * 0.48, 0, 2 * G_PI);
    cairo_set_source_rgb(cr,
                         piece == GNECT_RED ? 1.0 : 0.96,
                         piece == GNECT_RED ? 1.0 : 0.96,
                         piece == GNECT_RED ? 1.0 : 0.92);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, MAX(1.6, radius * 0.045));
    cairo_stroke(cr);

    cairo_set_source_rgb(cr,
                         0.0,
                         0.0,
                         0.0);
    draw_centered_text(cr, piece == GNECT_RED ? "R" : "Y", cx, cy, radius * 0.95);
}

static gboolean board_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    cairo_t *cr = gdk_cairo_create(widget->window);
    GtkAllocation allocation;
    int cell;
    int board_w;
    int board_h;
    int ox;
    int oy;
    int col;
    int row;

    (void)event;
    (void)data;

    gtk_widget_get_allocation(widget, &allocation);
    cell = MIN(allocation.width / GNECT_COLS, allocation.height / GNECT_ROWS);
    cell -= 4;
    board_w = cell * GNECT_COLS;
    board_h = cell * GNECT_ROWS;
    ox = (allocation.width - board_w) / 2;
    oy = (allocation.height - board_h) / 2;

    cairo_set_source_rgb(cr, 0.86, 0.84, 0.76);
    cairo_paint(cr);

    cairo_rectangle(cr, ox, oy, board_w, board_h);
    cairo_set_source_rgb(cr, 0.36, 0.45, 0.62);
    cairo_fill(cr);

    for (row = 0; row < GNECT_ROWS; row++) {
        for (col = 0; col < GNECT_COLS; col++) {
            double cx = ox + col * cell + cell / 2.0;
            double cy = oy + row * cell + cell / 2.0;
            GnectPiece piece = viewed_piece_at(col, row);

            cairo_arc(cr, cx, cy, cell * 0.39, 0, 2 * G_PI);
            cairo_set_source_rgb(cr, 0.92, 0.90, 0.82);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0.08, 0.10, 0.16);
            cairo_set_line_width(cr, 1.2);
            cairo_stroke(cr);

            if (piece != GNECT_EMPTY) {
                draw_piece(cr, piece, cx, cy, cell * 0.34);
            }
        }
    }

    if (is_viewing_latest() && !is_ai_turn() && app.game.winner == GNECT_EMPTY) {
        int moves[GNECT_COLS];
        int count = gnect_collect_valid_moves(&app.game, moves);
        int i;
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        for (i = 0; i < count; i++) {
            col = moves[i];
            cairo_move_to(cr, ox + col * cell + cell / 2.0, oy - 4);
            cairo_line_to(cr, ox + col * cell + cell * 0.35, oy - 24);
            cairo_line_to(cr, ox + col * cell + cell * 0.65, oy - 24);
            cairo_close_path(cr);
            cairo_fill(cr);
        }
    }

    cairo_destroy(cr);
    return FALSE;
}

static void apply_move_and_update(int col)
{
    GnectMoveState state = gnect_apply_move(&app.game, col);
    app.view_ply = -1;

    if (state == GNECT_WIN)
        snprintf(app.message, sizeof(app.message), "Game over. %s wins.", piece_name(app.game.winner));
    else if (state == GNECT_DRAW)
        set_message("Game over. Draw.");
    else if (state == GNECT_RUNNING)
        snprintf(app.message, sizeof(app.message), "%s to move.", piece_name(app.game.turn));
    else
        set_message("Invalid move.");

    update_ui();
}

static gboolean board_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    GtkAllocation allocation;
    int cell;
    int board_w;
    int ox;
    int col;

    (void)data;

    if (event->button != 1 || !is_viewing_latest() || is_ai_turn() || app.game.winner != GNECT_EMPTY)
        return FALSE;

    gtk_widget_get_allocation(widget, &allocation);
    cell = MIN(allocation.width / GNECT_COLS, allocation.height / GNECT_ROWS) - 4;
    board_w = cell * GNECT_COLS;
    ox = (allocation.width - board_w) / 2;
    col = ((int)event->x - ox) / cell;

    if (!gnect_is_valid_column(&app.game, col)) {
        set_message("Column is full.");
        update_ui();
        return TRUE;
    }

    apply_move_and_update(col);
    return TRUE;
}

static gboolean ai_timeout(gpointer data)
{
    int col = -1;

    (void)data;
    app.ai_source = 0;

    if (!is_ai_turn())
        return FALSE;

    gnect_ai_pick_move(&app.game, app.level, &col);
    if (col >= 0)
        apply_move_and_update(col);
    else
        update_ui();

    return FALSE;
}

static void update_history_label(void)
{
    GString *text = g_string_new("");
    int view_ply = current_view_ply();
    int i;

    g_string_append_printf(text, "%c Start\n", view_ply == 0 ? '>' : ' ');
    for (i = 0; i < app.game.move_count; i++)
        g_string_append_printf(text, "%c%2d. %s %d\n", view_ply == i + 1 ? '>' : ' ',
                               i + 1, i % 2 == 0 ? "Red" : "Yellow", app.game.moves[i] + 1);

    gtk_label_set_text(GTK_LABEL(app.moves_label), text->str);
    g_string_free(text, TRUE);
}

static void update_ui(void)
{
    char buf[160];
    int view_ply = current_view_ply();
    GnectGame viewed = app.game;

    memcpy(viewed.board, app.game.history[view_ply], sizeof(viewed.board));

    if (is_viewing_latest()) {
        gtk_label_set_text(GTK_LABEL(app.status), app.message);
    } else {
        snprintf(buf, sizeof(buf), "Viewing move %d of %d. Tap >| to resume play.", view_ply, app.game.move_count);
        gtk_label_set_text(GTK_LABEL(app.status), buf);
    }

    snprintf(buf, sizeof(buf), "Red: %d", gnect_count_piece(&viewed, GNECT_RED));
    gtk_label_set_text(GTK_LABEL(app.red_score), buf);
    snprintf(buf, sizeof(buf), "Yellow: %d", gnect_count_piece(&viewed, GNECT_YELLOW));
    gtk_label_set_text(GTK_LABEL(app.yellow_score), buf);

    update_history_label();
    gtk_widget_queue_draw(app.board);

    gtk_widget_set_sensitive(app.history_first_button, view_ply > 0);
    gtk_widget_set_sensitive(app.history_prev_button, view_ply > 0);
    gtk_widget_set_sensitive(app.history_next_button, view_ply < app.game.move_count);
    gtk_widget_set_sensitive(app.history_latest_button, view_ply < app.game.move_count);

    if (is_ai_turn() && app.ai_source == 0)
        app.ai_source = g_timeout_add(300, ai_timeout, NULL);
}

static void new_game(void)
{
    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }
    gnect_game_init(&app.game);
    app.view_ply = -1;
    set_message("Red to move.");
    update_ui();
}

static void new_game_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    new_game();
}

static void undo_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;

    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }

    if (gnect_undo(&app.game)) {
        if (app.mode == MODE_PLAY_RED && app.game.turn == GNECT_YELLOW)
            gnect_undo(&app.game);
        else if (app.mode == MODE_PLAY_YELLOW && app.game.turn == GNECT_RED)
            gnect_undo(&app.game);
        app.view_ply = -1;
        snprintf(app.message, sizeof(app.message), "%s to move.", piece_name(app.game.turn));
    } else {
        set_message("Nothing to undo.");
    }
    update_ui();
}

static void quit_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    gtk_main_quit();
}

static void save_cb(GtkWidget *widget, gpointer data)
{
    FILE *f;
    int i;

    (void)widget;
    (void)data;

    f = fopen(SAVE_PATH, "w");
    if (!f) {
        set_message("Could not save game.");
        update_ui();
        return;
    }

    fprintf(f, "%d\n", app.game.move_count);
    for (i = 0; i < app.game.move_count; i++)
        fprintf(f, "%d\n", app.game.moves[i]);
    fclose(f);
    set_message("Saved to Documents/kindle-gnect.txt");
    update_ui();
}

static void load_cb(GtkWidget *widget, gpointer data)
{
    FILE *f;
    int move_count;
    int i;

    (void)widget;
    (void)data;

    f = fopen(SAVE_PATH, "r");
    if (!f) {
        set_message("No saved game found.");
        update_ui();
        return;
    }

    if (fscanf(f, "%d\n", &move_count) != 1 || move_count < 0 || move_count > GNECT_MAX_MOVES) {
        fclose(f);
        set_message("Save file is invalid.");
        update_ui();
        return;
    }

    gnect_game_init(&app.game);
    for (i = 0; i < move_count; i++) {
        int col;
        if (fscanf(f, "%d\n", &col) != 1 || gnect_apply_move(&app.game, col) == GNECT_INVALID) {
            fclose(f);
            new_game();
            set_message("Save file is invalid.");
            return;
        }
    }
    fclose(f);
    app.view_ply = -1;
    snprintf(app.message, sizeof(app.message), "Loaded. %s to move.", piece_name(app.game.turn));
    if (app.game.winner != GNECT_EMPTY)
        snprintf(app.message, sizeof(app.message), "Loaded. %s already won.", piece_name(app.game.winner));
    update_ui();
}

static void mode_changed(GtkComboBox *combo, gpointer data)
{
    (void)data;
    app.mode = (AppMode)gtk_combo_box_get_active(combo);
    new_game();
}

static void level_changed(GtkComboBox *combo, gpointer data)
{
    (void)data;
    app.level = gtk_combo_box_get_active(combo);
    if (app.level < 0)
        app.level = 1;
    snprintf(app.message, sizeof(app.message), "Difficulty changed. %s to move.", piece_name(app.game.turn));
    update_ui();
}

static void set_view_ply(int ply)
{
    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }

    if (ply < 0)
        ply = 0;
    if (ply > app.game.move_count)
        ply = app.game.move_count;

    app.view_ply = (ply == app.game.move_count) ? -1 : ply;
    update_ui();
}

static void history_first_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(0);
}

static void history_prev_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(current_view_ply() - 1);
}

static void history_next_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(current_view_ply() + 1);
}

static void history_latest_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(app.game.move_count);
}

static GtkWidget *combo_with_items(const char **items, int active)
{
    GtkWidget *combo = gtk_combo_box_new_text();
    int i;

    for (i = 0; items[i] != NULL; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo), items[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
    gtk_widget_set_size_request(combo, 300, -1);
    app_apply_high_contrast(combo);
    return combo;
}

static void labeled_combo(GtkWidget *row, const char *label_text, GtkWidget *combo)
{
    GtkWidget *box = gtk_hbox_new(FALSE, 4);
    GtkWidget *label = gtk_label_new(label_text);

    gtk_box_pack_start(GTK_BOX(row), box, TRUE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
    gtk_widget_set_size_request(label, 130, -1);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    (void)widget;
    (void)data;

    if (event->keyval == GDK_Escape || event->keyval == GDK_q) {
        gtk_main_quit();
        return TRUE;
    }
    if (event->keyval == GDK_r) {
        new_game();
        return TRUE;
    }
    if (event->keyval == GDK_u) {
        undo_cb(NULL, NULL);
        return TRUE;
    }
    return FALSE;
}

static GtkWidget *add_button(GtkWidget *box, const char *label, GCallback callback)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    g_signal_connect(button, "clicked", callback, NULL);
    return button;
}

static void build_ui(void)
{
    static const char *modes[] = {"Play Red", "Play Yellow", "2 Player", "AI Demo", NULL};
    static const char *levels[] = {"Easy", "Medium", "Hard", NULL};
    GtkWidget *vbox;
    GtkWidget *title;
    GtkWidget *controls;
    GtkWidget *settings;
    GtkWidget *content;
    GtkWidget *sidebar;
    GtkWidget *score_box;
    GtkWidget *history_frame;
    GtkWidget *history_scroll;
    GtkWidget *history_nav_box;

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), KINDLE_WINDOW_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(app.window), KINDLE_APP_WIDTH, KINDLE_APP_HEIGHT);
    gtk_widget_set_size_request(app.window, KINDLE_APP_WIDTH, KINDLE_APP_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(app.window), FALSE);
    gtk_window_move(GTK_WINDOW(app.window), 0, 0);
    gtk_container_set_border_width(GTK_CONTAINER(app.window), 8);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app.window, "key-press-event", G_CALLBACK(key_press), NULL);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(app.window), vbox);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Kindle Gnect</b>");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    app.status = gtk_label_new("Red to move.");
    gtk_misc_set_alignment(GTK_MISC(app.status), 0.5f, 0.5f);
    gtk_box_pack_start(GTK_BOX(vbox), app.status, FALSE, FALSE, 0);

    controls = gtk_hbox_new(TRUE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), controls, FALSE, FALSE, 0);
    add_button(controls, "New", G_CALLBACK(new_game_cb));
    add_button(controls, "Undo", G_CALLBACK(undo_cb));
    add_button(controls, "Save", G_CALLBACK(save_cb));
    add_button(controls, "Load", G_CALLBACK(load_cb));
    add_button(controls, "Quit", G_CALLBACK(quit_cb));

    settings = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(vbox), settings, FALSE, FALSE, 0);
    app.mode_combo = combo_with_items(modes, 0);
    labeled_combo(settings, "Mode", app.mode_combo);
    g_signal_connect(app.mode_combo, "changed", G_CALLBACK(mode_changed), NULL);
    app.level_combo = combo_with_items(levels, 1);
    labeled_combo(settings, "Level", app.level_combo);
    g_signal_connect(app.level_combo, "changed", G_CALLBACK(level_changed), NULL);

    content = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(vbox), content, TRUE, TRUE, 0);

    app.board = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.board, 760, 660);
    gtk_box_pack_start(GTK_BOX(content), app.board, FALSE, FALSE, 0);
    gtk_widget_add_events(app.board, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(app.board, "expose-event", G_CALLBACK(board_expose), NULL);
    g_signal_connect(app.board, "button-press-event", G_CALLBACK(board_button), NULL);

    sidebar = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(content), sidebar, FALSE, TRUE, 0);

    score_box = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(sidebar), score_box, FALSE, FALSE, 0);
    app.red_score = gtk_label_new("Red: 0");
    app.yellow_score = gtk_label_new("Yellow: 0");
    gtk_misc_set_alignment(GTK_MISC(app.red_score), 0.0f, 0.5f);
    gtk_misc_set_alignment(GTK_MISC(app.yellow_score), 0.0f, 0.5f);
    gtk_box_pack_start(GTK_BOX(score_box), app.red_score, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(score_box), app.yellow_score, FALSE, FALSE, 0);

    history_frame = gtk_frame_new("Moves");
    gtk_box_pack_start(GTK_BOX(sidebar), history_frame, TRUE, TRUE, 0);
    history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(history_frame), history_scroll);
    gtk_widget_set_size_request(history_scroll, 260, 620);
    app.moves_label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(app.moves_label), 0.0f, 0.0f);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(history_scroll), app.moves_label);

    history_nav_box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar), history_nav_box, FALSE, FALSE, 0);
    app.history_first_button = add_button(history_nav_box, "|<", G_CALLBACK(history_first_cb));
    app.history_prev_button = add_button(history_nav_box, "<", G_CALLBACK(history_prev_cb));
    app.history_next_button = add_button(history_nav_box, ">", G_CALLBACK(history_next_cb));
    app.history_latest_button = add_button(history_nav_box, ">|", G_CALLBACK(history_latest_cb));
}

int main(int argc, char **argv)
{
    (void)argv;

    app_log("startup");
    gtk_init(&argc, &argv);
    app_install_kindle_style();

    app.mode = MODE_PLAY_RED;
    app.level = 1;
    app.view_ply = -1;
    gnect_game_init(&app.game);
    set_message("Red to move.");
    build_ui();
    update_ui();
    gtk_widget_show_all(app.window);
    gtk_window_present(GTK_WINDOW(app.window));
    app_log("window shown");
    gtk_main();
    app_log("shutdown");

    return 0;
}
