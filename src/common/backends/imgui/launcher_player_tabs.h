// launcher_player_tabs.h - the tab strip over the dashboard's controller card:
// one tab per player, and one player's card under it.
//
// The cards used to stand side by side. A title with four players then needed
// two rows of them, and the second row did not fit above the memory cards:
// "PLAYER 4" was cut off and the section scrolled. With a tab per player the
// section is one card high whatever the player count.
//
// In a header, as launcher_nav.h and launcher_pad_nav.h are, so that
// tests/launcher_player_tabs_test.cpp can run it in real ImGui frames with no
// window.
//
// How a tab is selected:
//   - a click;
//   - focus (d-pad or arrow keys along the strip), then activate (Cross / A,
//     Enter, Space). Focus alone selects nothing: the focus ring crosses the
//     strip on its way up or down the page, and that must not change which
//     player is shown;
//   - L1 / R1 from anywhere on the page: the previous / next player, with the
//     focus ring put on that tab.
#pragma once

#include "imgui.h"

#include <cstdio>

// Colours of the active theme and sizes in logical units.
struct RuiPlayerTabsLook {
    ImU32 tab, tab_hovered, tab_selected;   // fills
    ImU32 border;                           // outline of the selected tab
    ImU32 text, text_selected;
    ImU32 accent;                           // bar under the selected tab
    ImU32 dot_on, dot_off;                  // the player has a device / has none
    float height, gap, rounding;
};

// L1 / R1 as "previous / next player": -1, +1 or 0.
//
// Not while a field is edited or an item is held, and not over an open list
// or dialog (the rule of rui_nav_rehome_to_play). Not while face-left is down
// either: that is ImGui's window-switcher button, and it gives L1 / R1 another
// meaning. IsKeyPressed honours the lock that launcher_pad_nav_lock_held_keys()
// takes, so a shoulder button that is held when pad navigation starts does
// nothing until it is released; and the backend feeds no pad key at all while
// pad navigation is off.
inline int rui_player_tabs_shoulder_step() {
    if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive()) return 0;
    const char* any = nullptr;   // typed: imgui_internal.h adds an ImGuiID overload
    if (ImGui::IsPopupOpen(any, ImGuiPopupFlags_AnyPopupId |
                                ImGuiPopupFlags_AnyPopupLevel))
        return 0;
    if (ImGui::IsKeyDown(ImGuiKey_GamepadFaceLeft)) return 0;
    return (int)ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) -
           (int)ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false);
}

// Draws `count` equal tabs across `width` at the cursor and returns the
// selected player (0-based) after this frame's input.
//
// assigned[p] puts the card's own dot on tab p (filled: the player has a
// device; ring: "not assigned"), so nobody has to open every tab to see who
// plays. The label is "<word> N"; when that does not fit the tab it is
// "<short_word>N", so eight tabs still fit the narrowest column.
inline int rui_player_tabs(int count, int selected, const bool* assigned,
                           float width, const RuiPlayerTabsLook& look,
                           const char* word, const char* short_word) {
    if (count < 1) return 0;
    if (selected < 0) selected = 0;
    if (selected >= count) selected = count - 1;

    const int step = rui_player_tabs_shoulder_step();
    if (step) selected = (selected + count + step) % count;
    int picked = selected;

    const float tab_w = (width - look.gap * (float)(count - 1)) / (float)count;
    const float dot_r = 5.0f, dot_gap = 6.0f, pad = 8.0f;
    const float text_h = ImGui::GetTextLineHeight();

    // One label form for the whole strip, chosen by its widest label.
    char label[48];
    std::snprintf(label, sizeof(label), "%s %d", word, count);
    const bool full = dot_r * 2.0f + dot_gap + ImGui::CalcTextSize(label).x +
                      pad * 2.0f <= tab_w;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int p = 0; p < count; ++p) {
        if (p) ImGui::SameLine(0, look.gap);
        ImGui::PushID(p);
        const ImVec2 mn = ImGui::GetCursorScreenPos();
        const ImVec2 mx(mn.x + tab_w, mn.y + look.height);
        if (ImGui::InvisibleButton("##player_tab", ImVec2(tab_w, look.height),
                                   ImGuiButtonFlags_EnableNav))
            picked = p;
        /* -1: the item just submitted. It only focuses a button. */
        if (step && p == selected) ImGui::SetKeyboardFocusHere(-1);
        const bool sel = p == selected;
        const bool hov = ImGui::IsItemHovered();

        dl->AddRectFilled(mn, mx, sel ? look.tab_selected
                                      : hov ? look.tab_hovered : look.tab,
                          look.rounding);
        if (sel) {
            dl->AddRect(mn, mx, look.border, look.rounding);
            dl->AddRectFilled(ImVec2(mn.x + look.rounding, mx.y - 3.0f),
                              ImVec2(mx.x - look.rounding, mx.y), look.accent, 1.5f);
        }

        if (full) std::snprintf(label, sizeof(label), "%s %d", word, p + 1);
        else      std::snprintf(label, sizeof(label), "%s%d", short_word, p + 1);
        const float text_w = ImGui::CalcTextSize(label).x;
        const float x = mn.x + (tab_w - (dot_r * 2.0f + dot_gap + text_w)) * 0.5f;
        const ImVec2 dot(x + dot_r, mn.y + look.height * 0.5f);
        if (assigned && assigned[p]) dl->AddCircleFilled(dot, dot_r, look.dot_on);
        else                         dl->AddCircle(dot, dot_r, look.dot_off, 0, 1.5f);
        dl->AddText(ImVec2(x + dot_r * 2.0f + dot_gap,
                           mn.y + (look.height - text_h) * 0.5f),
                    sel ? look.text_selected : look.text, label);
        ImGui::PopID();
    }
    return picked;
}
