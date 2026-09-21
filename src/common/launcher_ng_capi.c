// launcher_ng_capi.c — the C ABI the game calls, backed by the launcher_ng UI.
//
// Implements recomp_launcher_run_window() (declared in recomp_launcher.h),
// the generic C ABI a host app calls to run the pre-boot launcher UI. Hosts
// seed the C structs, call the function, and read back the chosen ROM +
// settings.
//
// Returns: 0 = LAUNCH, 1 = QUIT, 2 = UNAVAILABLE, 3 = RELAUNCH.

#include "recomp_launcher.h"

#include "launcher_backend.h"
#include "launcher_binds.h"
#include "launcher_boot_timing.h"
#include "launcher_files.h"
#include "launcher_model.h"
#include "launcher_platform.h"
#include "launcher_theme.h"

#include <stdio.h>
#include <string.h>

static char g_last_relaunch_exe[512];

void recomp_launcher_set_preserve_sdl(int preserve) {
    launcher_platform_set_quit_sdl(!preserve);
}

int recomp_launcher_relaunch_exe(char* out, size_t out_cap) {
    if (!out || out_cap == 0 || !g_last_relaunch_exe[0])
        return 0;
    snprintf(out, out_cap, "%s", g_last_relaunch_exe);
    return 1;
}

int recomp_launcher_run_window(const char* window_title,
                             RecompLauncherCSettings* io,
                             const RecompLauncherCGameInfo* game,
                             const char* assets_dir,
                             const char* initial_rom,
                             char* out_rom_path, size_t out_rom_path_len) {
    (void)assets_dir;   // launcher_ng resolves assets next to the exe (SDL base path)
    g_last_relaunch_exe[0] = '\0';

    /* Packaging self-test (RECOMP_UI_PICKER_SELFTEST): exercise the native
     * file picker and report the outcome, before any window/GL work so it
     * runs headless. Then take the no-launcher path, exactly as if the
     * launcher window had been unavailable. */
    if (launcher_file_picker_selftest())
        return RECOMP_LAUNCHER_RESULT_UNAVAILABLE;

    launcher_boot_timing_mark("rui:run_window:enter");

    LauncherPlatform plat;
    if (!launcher_platform_open(&plat, window_title ? window_title : "Launcher",
                                1100, 880)) {
        // Window/GL init failed — tell the caller to boot as if the launcher was
        // skipped, exactly like the old launcher's UNAVAILABLE path.
        return RECOMP_LAUNCHER_RESULT_UNAVAILABLE;
    }

    /* Same icon the host's game window carries — see GameInfo.window_icon_path.
     * Applied before the model is built so the window is never briefly shown
     * under the placeholder icon. */
    launcher_platform_set_icon(&plat, game ? game->window_icon_path : NULL);

    LauncherModel model;
    launcher_model_init(&model, io, game, initial_rom);
    launcher_binds_load(&model, game ? game->config_path : NULL,
                                game ? game->keybinds_path : NULL);
    launcher_boot_timing_mark("rui:model+binds_ready");

    LauncherTheme theme = launcher_theme_by_name(game ? game->theme : NULL);

    LngAction act = launcher_backend_run(&plat, &model, &theme);

    launcher_platform_close(&plat);

    /* Edited settings go back to the caller on EVERY exit, quit included.
     *
     * Commit used to run only on LAUNCH/RELAUNCH, so a player who opened the
     * launcher, changed Fullscreen or a controller source, and then closed the
     * window threw the whole edit away -- the host still held the values it
     * seeded, wrote those back to its config, and the next run reopened on the
     * old settings. The launcher's own direct-write stores (keybinds.ini,
     * [KeyMap], [GamepadMap]) already persist on quit; the settings
     * struct was the one surface that did not. A setting changed and then
     * dismissed is still a setting changed. Commit also persists verified
     * dashboard cartridge picks; those hosts never enter the setup wizard's
     * sidecar-writing path. */
    launcher_model_commit(&model, io);   // edited settings back to the caller
    if (act != LNG_ACTION_LAUNCH && act != LNG_ACTION_RELAUNCH && io) {
        /* netplay_launch is a transient OUTPUT, not a setting: only a real
         * lobby launch may arm it. Quitting must never hand the host a
         * pending session. */
        memset(&io->netplay_launch, 0, sizeof(io->netplay_launch));
    }

    if (act == LNG_ACTION_LAUNCH || act == LNG_ACTION_RELAUNCH) {
        const char* rom = launcher_model_effective_rom_path(&model);
        if (out_rom_path && out_rom_path_len) {
            if (rom && rom[0])
                snprintf(out_rom_path, out_rom_path_len, "%s", rom);
            else if (initial_rom)
                snprintf(out_rom_path, out_rom_path_len, "%s", initial_rom);
            else
                out_rom_path[0] = '\0';
        }
        if (act == LNG_ACTION_RELAUNCH) {
            if (model.relaunch_exe[0])
                snprintf(g_last_relaunch_exe, sizeof(g_last_relaunch_exe),
                         "%s", model.relaunch_exe);
            return RECOMP_LAUNCHER_RESULT_RELAUNCH;
        }
        return RECOMP_LAUNCHER_RESULT_LAUNCH;
    }

    return RECOMP_LAUNCHER_RESULT_QUIT;
}
