// The dashboard's player tab strip (launcher_player_tabs.h) in real ImGui
// frames with no window, as launcher_pad_nav_test.cpp runs the pad lock.
//
// What a player relies on:
//   - there is one tab per player, and a click opens it;
//   - the focus ring can cross the strip without changing the player: only
//     activate (Space, Cross / A) opens the focused tab;
//   - R1 / L1 open the next / previous player and put the focus ring there;
//   - a shoulder button that is held when pad navigation starts does nothing
//     (the lock of launcher_pad_nav.h), and nothing happens over an open list,
//     in a text field, or while the window-switcher button is down.
#include "imgui.h"
#include "imgui_internal.h"
#include "launcher_pad_nav.h"
#include "launcher_player_tabs.h"

#include <cstdio>

static int fails;

static void expect(bool cond, const char* what) {
    if (cond) { std::printf("ok: %s\n", what); return; }
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++fails;
}

static const RuiPlayerTabsLook kLook = {
    IM_COL32(30, 30, 40, 255), IM_COL32(40, 40, 50, 255), IM_COL32(20, 20, 30, 255),
    IM_COL32(60, 60, 80, 255), IM_COL32(140, 140, 160, 255), IM_COL32(230, 230, 240, 255),
    IM_COL32(46, 125, 255, 255), IM_COL32(70, 227, 155, 255), IM_COL32(140, 140, 160, 255),
    34.0f, 4.0f, 6.0f,
};

struct Page {
    int     count = 4;
    int     open = 0;          // the strip's answer, fed back every frame
    float   width = 640.0f;
    bool    list_open = false; // a popup over the page
    bool    field = false;     // a text field that holds the keyboard
    ImVec2  origin;
    ImGuiID tab_id[8] = {};
    ImGuiID card_id = 0;
    char    name[32] = "pad";
};

static void new_context() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    unsigned char* px = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
}

// One dashboard-like frame: a button above the strip, the strip, and a button
// under it where the player's card is.
static void frame(Page& pg) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1280, 720));
    ImGui::Begin("##page", nullptr, ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::Button("Settings");
    pg.origin = ImGui::GetCursorScreenPos();
    bool assigned[8] = { true };
    pg.open = rui_player_tabs(pg.count, pg.open, assigned, pg.width, kLook, "Player", "P");
    for (int p = 0; p < pg.count; ++p) {
        ImGui::PushID(p);
        pg.tab_id[p] = ImGui::GetID("##player_tab");
        ImGui::PopID();
    }
    ImGui::Button("Configure");
    pg.card_id = ImGui::GetItemID();
    if (pg.field) {
        ImGui::SetKeyboardFocusHere();
        ImGui::InputText("##name", pg.name, sizeof(pg.name));
    }
    if (pg.list_open) {
        ImGui::OpenPopup("##list");
        if (ImGui::BeginPopup("##list")) {
            ImGui::Selectable("Keyboard");
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    ImGui::EndFrame();
}

static void frames(Page& pg, int n) { for (int i = 0; i < n; ++i) frame(pg); }

static void tap(Page& pg, ImGuiKey key) {
    ImGui::GetIO().AddKeyEvent(key, true);
    frames(pg, 2);
    ImGui::GetIO().AddKeyEvent(key, false);
    frames(pg, 2);
}

static void click(Page& pg, float x, float y) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    frames(pg, 2);
    io.AddMouseButtonEvent(0, true);
    frames(pg, 2);
    io.AddMouseButtonEvent(0, false);
    frames(pg, 2);
}

static void click_tab(Page& pg, int p) {
    const float tab_w = (pg.width - kLook.gap * (float)(pg.count - 1)) / (float)pg.count;
    click(pg, pg.origin.x + (tab_w + kLook.gap) * (float)p + tab_w * 0.5f,
          pg.origin.y + kLook.height * 0.5f);
}

static ImGuiID focus() { return GImGui->NavId; }

static void test_one_tab_per_player() {
    static const int kCounts[] = { 2, 4, 8 };
    for (int count : kCounts) {
        new_context();
        Page pg;
        pg.count = count;
        frames(pg, 2);
        bool each = true;
        for (int p = count - 1; p >= 0; --p) {
            click_tab(pg, p);
            if (pg.open != p) each = false;
        }
        char what[64];
        std::snprintf(what, sizeof(what), "%d players: a click opens each of %d tabs", count, count);
        expect(each, what);
        click_tab(pg, count - 1);
        click(pg, pg.origin.x + pg.width + 40.0f, pg.origin.y + kLook.height * 0.5f);
        std::snprintf(what, sizeof(what), "%d players: there is no tab past the last one", count);
        expect(pg.open == count - 1, what);
        ImGui::DestroyContext();
    }

    // A tab that is not there cannot be the open one.
    new_context();
    Page pg;
    pg.count = 2;
    pg.open = 5;
    frames(pg, 2);
    expect(pg.open == 1, "an open tab past the strip becomes the last tab");
    ImGui::DestroyContext();
}

static void test_focus_does_not_open() {
    new_context();
    Page pg;
    frames(pg, 2);
    click_tab(pg, 0);
    expect(pg.open == 0 && focus() == pg.tab_id[0], "a click opens Player 1 and focuses its tab");

    tap(pg, ImGuiKey_RightArrow);
    expect(focus() == pg.tab_id[1], "Right moves the focus ring to Player 2's tab");
    expect(pg.open == 0, "and Player 1 stays open");
    tap(pg, ImGuiKey_Space);
    expect(pg.open == 1, "Space opens the focused tab");

    tap(pg, ImGuiKey_RightArrow);
    tap(pg, ImGuiKey_RightArrow);
    expect(focus() == pg.tab_id[3], "Right twice more reaches Player 4's tab");
    tap(pg, ImGuiKey_GamepadFaceDown);
    expect(pg.open == 3, "Cross / A opens the focused tab");

    // Down to the card and back up: the ring lands on a tab, the player stays.
    tap(pg, ImGuiKey_DownArrow);
    expect(focus() == pg.card_id, "Down leaves the strip for the card");
    tap(pg, ImGuiKey_UpArrow);
    bool on_a_tab = false;
    for (int p = 0; p < pg.count; ++p) on_a_tab = on_a_tab || focus() == pg.tab_id[p];
    expect(on_a_tab, "Up comes back to the strip");
    expect(pg.open == 3, "crossing the strip does not change the open player");
    ImGui::DestroyContext();
}

static void test_shoulder_buttons() {
    new_context();
    Page pg;
    frames(pg, 2);
    tap(pg, ImGuiKey_GamepadR1);
    expect(pg.open == 1, "R1 opens the next player");
    expect(focus() == pg.tab_id[1], "and puts the focus ring on that tab");
    tap(pg, ImGuiKey_GamepadR1);
    tap(pg, ImGuiKey_GamepadR1);
    expect(pg.open == 3, "R1 twice more opens Player 4");
    tap(pg, ImGuiKey_GamepadR1);
    expect(pg.open == 0, "R1 on the last player goes round to Player 1");
    tap(pg, ImGuiKey_GamepadL1);
    expect(pg.open == 3, "L1 on Player 1 goes round to the last player");
    tap(pg, ImGuiKey_GamepadL1);
    expect(pg.open == 2 && focus() == pg.tab_id[2], "L1 opens the previous player");

    // Held for a second: one step, not one a frame.
    ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadR1, true);
    frames(pg, 60);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadR1, false);
    frames(pg, 2);
    expect(pg.open == 3, "a held R1 steps once");
    ImGui::DestroyContext();
}

static void test_shoulder_buttons_stay_out_of_the_way() {
    // Held when pad navigation starts: nothing until it is released.
    new_context();
    Page pg;
    frames(pg, 2);
    launcher_pad_nav_lock_held_keys();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadR1, true);
    frames(pg, 30);
    expect(pg.open == 0, "an R1 held when pad navigation starts opens nothing");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadR1, false);
    frames(pg, 3);
    tap(pg, ImGuiKey_GamepadR1);
    expect(pg.open == 1, "after a release, a real press works");
    ImGui::DestroyContext();

    // The window-switcher button gives L1 / R1 another meaning.
    new_context();
    pg = Page();
    frames(pg, 2);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadFaceLeft, true);
    frames(pg, 3);
    tap(pg, ImGuiKey_GamepadR1);
    expect(pg.open == 0, "R1 with face-left held opens nothing");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadFaceLeft, false);
    frames(pg, 3);
    ImGui::DestroyContext();

    // An open list owns the pad.
    new_context();
    pg = Page();
    pg.list_open = true;
    frames(pg, 3);
    tap(pg, ImGuiKey_GamepadR1);
    expect(pg.open == 0, "R1 over an open list opens nothing");
    ImGui::DestroyContext();

    // So does a text field.
    new_context();
    pg = Page();
    pg.field = true;
    frames(pg, 3);
    tap(pg, ImGuiKey_GamepadR1);
    expect(pg.open == 0, "R1 while a field is edited opens nothing");
    ImGui::DestroyContext();
}

int main() {
    IMGUI_CHECKVERSION();
    test_one_tab_per_player();
    test_focus_does_not_open();
    test_shoulder_buttons();
    test_shoulder_buttons_stay_out_of_the_way();
    if (fails) { std::fprintf(stderr, "launcher_player_tabs_strip_test: %d failure(s)\n", fails); return 1; }
    std::printf("launcher_player_tabs_strip_test: all checks passed\n");
    return 0;
}
