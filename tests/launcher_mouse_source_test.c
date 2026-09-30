/* The "PS1 Mouse" input source (player_src 3, RECOMP_LAUNCHER_SRC_MOUSE).
 *
 * psxrecomp can put a Sony PS1 Mouse in a controller port, driven by the host
 * pointer. The launcher offers it as a fourth source, but only for the PSX
 * profile: no other console here has a mouse port device, so everywhere else
 * a 3 must read as None and never be stored.
 *
 * Includes the model translation unit, as launcher_pad_mode_test.c does.
 */
#include "launcher_model.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void launcher_binds_set_zapper(int a, int b);
void launcher_binds_set_zapper(int a, int b) { (void)a; (void)b; }

static int fails;

static void expect(int cond, const char* what) {
    if (cond) { printf("ok: %s\n", what); return; }
    fprintf(stderr, "FAIL: %s\n", what);
    ++fails;
}

enum { SRC_NONE = 0, SRC_KEYBOARD = 1, SRC_GAMEPAD = 2 };

static LauncherModel* session(int psx, int p0_src) {
    static RecompLauncherCGameInfo game;
    static RecompLauncherCSettings io;
    LauncherModel* m = (LauncherModel*)calloc(1, sizeof(LauncherModel));
    if (!m) { fprintf(stderr, "FAIL: out of memory\n"); ++fails; return NULL; }
    memset(&game, 0, sizeof(game));
    if (psx) launcher_profile_apply_psx(&game);
    else launcher_profile_apply_genesis(&game);
    game.name = "Mouse Source Fixture";
    game.num_players = 2;
    game.pad_mode_selectable = 1;
    memset(&io, 0, sizeof(io));
    io.player_src[0] = p0_src;
    io.player_src[1] = SRC_NONE;
    launcher_model_init(m, &io, &game, NULL);
    return m;
}

static void test_psx_offers_mouse(void) {
    LauncherModel* m = session(1, SRC_KEYBOARD);
    if (!m) return;
    expect(launcher_model_mouse_source_available(m), "PSX offers the mouse source");
    launcher_model_set_source(m, 0, RECOMP_LAUNCHER_SRC_MOUSE, 0, NULL, NULL);
    expect(m->s.player_src[0] == RECOMP_LAUNCHER_SRC_MOUSE, "set_source stores 3 on PSX");
    expect(strcmp(launcher_model_player_src_label(m, 0), "PS1 Mouse") == 0,
           "the label reads PS1 Mouse");
    expect(m->s.player_gamepad_guid[0][0] == '\0', "a mouse seat carries no pad GUID");
    launcher_model_set_source(m, 0, SRC_NONE, 0, NULL, NULL);
    expect(m->s.player_src[0] == SRC_NONE, "choosing None clears the mouse");
    free(m);
}

static void test_host_seeded_mouse_survives_init(void) {
    /* psxrecomp seeds player_src = 3 from settings.toml pN_device = "mouse". */
    LauncherModel* m = session(1, RECOMP_LAUNCHER_SRC_MOUSE);
    if (!m) return;
    expect(m->s.player_src[0] == RECOMP_LAUNCHER_SRC_MOUSE,
           "a host-seeded mouse seat survives launcher_model_init");
    expect(strcmp(launcher_model_player_src_label(m, 0), "PS1 Mouse") == 0,
           "and is labelled PS1 Mouse");
    free(m);
}

static void test_cycle_includes_mouse_on_psx(void) {
    LauncherModel* m = session(1, SRC_GAMEPAD);
    if (!m) return;
    launcher_model_cycle_player_src(m, 0);
    expect(m->s.player_src[0] == RECOMP_LAUNCHER_SRC_MOUSE, "PSX cycle: Gamepad -> Mouse");
    launcher_model_cycle_player_src(m, 0);
    expect(m->s.player_src[0] == SRC_NONE, "PSX cycle: Mouse -> None");
    free(m);
}

static void test_other_profiles_refuse_mouse(void) {
    LauncherModel* m = session(0, SRC_GAMEPAD);
    if (!m) return;
    expect(!launcher_model_mouse_source_available(m), "Genesis offers no mouse source");
    launcher_model_set_source(m, 0, RECOMP_LAUNCHER_SRC_MOUSE, 0, NULL, NULL);
    expect(m->s.player_src[0] == SRC_NONE, "set_source maps 3 to None off PSX");
    launcher_model_cycle_player_src(m, 0);
    launcher_model_cycle_player_src(m, 0);
    launcher_model_cycle_player_src(m, 0);
    expect(m->s.player_src[0] == SRC_NONE, "non-PSX cycle stays None/Kbd/Pad");
    m->s.player_src[1] = RECOMP_LAUNCHER_SRC_MOUSE;   /* a stray value from a host */
    expect(strcmp(launcher_model_player_src_label(m, 1), "None") == 0,
           "a stray 3 is labelled None off PSX");
    free(m);
}

int main(void) {
    test_psx_offers_mouse();
    test_host_seeded_mouse_survives_init();
    test_cycle_includes_mouse_on_psx();
    test_other_profiles_refuse_mouse();
    if (fails) { fprintf(stderr, "launcher_mouse_source_test: %d failure(s)\n", fails); return 1; }
    printf("launcher_mouse_source_test: all checks passed\n");
    return 0;
}
