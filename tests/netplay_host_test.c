/*
 * recomp_netplay_host.c's launch planning, driven directly.
 *
 * Includes the module's translation unit (so the static seat planner and the
 * LAN launch are the real ones) and links recomp-net. Nothing here opens a
 * socket: no case connects, creates or joins.
 *
 * Built only when RECOMP_UI_RECOMP_NET_DIR names a recomp-net checkout
 * (see CMakeLists.txt); recomp-ui itself does not need recomp-net.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/netplay/recomp_netplay_host.c"

static int fails;

static void ck(int cond, const char *what)
{
    if (!cond) { printf("    FAIL %s\n", what); fails++; }
}

static void hooks_with(int policy, int max_players)
{
    RecompNetplayHostHooks h;
    memset(&h, 0, sizeof(h));
    h.game_name = "Test Game";
    h.game_version = "1.2.3";
    h.platform = "test";
    h.max_players = max_players;
    h.slot_policy = policy;
    ck(recomp_netplay_host_init(&h) == 0, "init");
}

/* The contract case: host is session slot 0 whatever seat it holds. */
static void case_host_first_standard_and_swapped(void)
{
    const int seats[2] = { 0, 1 };
    SlotPlan p;
    printf("  host first: two seats\n");
    hooks_with(RECOMP_NETPLAY_SLOTS_HOST_FIRST, 4);

    plan_session_slots(seats, 2, 0, 0, 1, &p);
    ck(p.local_slot == 0 && p.slot_count == 2 && p.occupied == 0x3u,
       "host in seat 0 is slot 0 of two");
    ck(p.port[0] == 0 && p.port[1] == 1 && p.port[2] == -1, "ports by seat");
    plan_session_slots(seats, 2, 0, 1, 0, &p);
    ck(p.local_slot == 1, "the guest in seat 1 is slot 1");

    /* The host moved to seat 1. It stays slot 0 and drives port 1. */
    plan_session_slots(seats, 2, 1, 1, 1, &p);
    ck(p.local_slot == 0, "a host in seat 1 is still session slot 0");
    ck(p.port[0] == 1 && p.port[1] == 0, "and drives its seat's port");
    plan_session_slots(seats, 2, 1, 0, 0, &p);
    ck(p.local_slot == 1, "the guest in seat 0 is slot 1");
}

static void case_host_first_sparse_four(void)
{
    const int seats[3] = { 3, 0, 1 };  /* any order */
    SlotPlan p;
    printf("  host first: sparse four-seat room\n");
    hooks_with(RECOMP_NETPLAY_SLOTS_HOST_FIRST, 4);
    plan_session_slots(seats, 3, 0, 3, 0, &p);
    ck(p.slot_count == 3, "three players, three slots (no hole)");
    ck(p.occupied == 0x7u, "every slot occupied");
    ck(p.local_slot == 2, "seat 3 is session slot 2");
    ck(p.port[0] == 0 && p.port[1] == 1 && p.port[2] == 3,
       "and drives port 3, where the lobby seated it");
    ck(p.port[3] == -1, "no fourth slot");
}

static void case_host_first_host_in_gallery(void)
{
    const int seats[2] = { 0, 1 };
    SlotPlan p;
    printf("  host first: host in the gallery\n");
    hooks_with(RECOMP_NETPLAY_SLOTS_HOST_FIRST, 4);
    plan_session_slots(seats, 2, -1, -1, 1, &p);
    ck(p.local_slot == 0, "the host still runs slot 0");
    ck(p.port[0] == -1, "with no port");
    ck(p.slot_count == 3 && p.port[1] == 0 && p.port[2] == 1,
       "players sit at seat + 1");
    plan_session_slots(seats, 2, -1, 1, 0, &p);
    ck(p.local_slot == 2, "the player in seat 1 is slot 2");
}

/* Legacy: session slot == seat. Complete, but the seat-0 player is the
 * authority -- the reason HOST_FIRST is the default. */
static void case_seat_policy(void)
{
    const int two[2] = { 0, 1 };
    const int sparse[2] = { 2, 0 };
    SlotPlan p;
    printf("  seat policy\n");
    hooks_with(RECOMP_NETPLAY_SLOTS_SEAT, 4);
    plan_session_slots(two, 2, 0, 0, 1, &p);
    ck(p.local_slot == 0 && p.slot_count == 2 && p.occupied == 0x3u,
       "host seat 0 -> slot 0");
    ck(p.port[0] == 0 && p.port[1] == 1, "identity ports");
    plan_session_slots(two, 2, 1, 1, 1, &p);
    ck(p.local_slot == 1, "a host in seat 1 is slot 1 (legacy)");
    plan_session_slots(sparse, 2, 0, 2, 0, &p);
    ck(p.local_slot == 2 && p.slot_count == 3, "seat 2 -> slot 2 of 3");
    ck(p.occupied == 0x5u && p.port[1] == -1, "the hole stays a hole");
}

/* The LAN room is two seats and carries only a delay. */
static void case_lan_launch(void)
{
    RNetLanLobby room;
    printf("  LAN launch\n");
    memset(&room, 0, sizeof(room));
    snprintf(room.endpoint, sizeof(room.endpoint), "%s", "192.168.1.5:7777");
    room.host_slot = 1;          /* the host swapped into seat 1 */
    room.input_delay = 5;

    hooks_with(RECOMP_NETPLAY_SLOTS_HOST_FIRST, 4);
    g_hosting_lan = 1;
    arm_lan_launch(&room);
    ck(g_lan_launch.enabled && g_lan_launch.local_slot == 0,
       "host first: the LAN host is slot 0");
    ck(g_lan_launch.slot_port_valid && g_lan_launch.slot_port[0] == 1 &&
       g_lan_launch.slot_port[1] == 0, "driving seat 1's port");
    ck(g_lan_launch.occupied_mask == 0x3u, "both seats occupied");
    ck(!strcmp(g_lan_launch.bind_hostport, "0.0.0.0:7777"), "host binds its port");
    ck(g_lan_launch.input_delay == 5, "the room's delay");
    ck(g_lan_launch.rollback == 0 && g_lan_launch.input_prediction == 0,
       "no mode is invented: the LAN room settles none");
    g_hosting_lan = 0;

    hooks_with(RECOMP_NETPLAY_SLOTS_SEAT, 2);
    g_hosting_lan = 1;
    arm_lan_launch(&room);
    ck(g_lan_launch.local_slot == 1, "seat policy: the LAN host is slot 1");
    ck(g_lan_launch.slot_port[1] == 1 && g_lan_launch.slot_port[0] == 0,
       "identity ports");
    g_hosting_lan = 0;
    g_joined_lan = 1;
    arm_lan_launch(&room);
    ck(g_lan_launch.local_slot == 0, "seat policy: the LAN guest is slot 0");
    ck(!strcmp(g_lan_launch.peer_hostport, "192.168.1.5:7777"),
       "the guest dials the room endpoint");
    g_joined_lan = 0;
}

static int fill_calls;
static void fill_hook(void *ctx, const RecompLauncherCSettings *settings,
                      RNetLobbyMatchCaps *caps)
{
    (void)ctx; (void)settings;
    fill_calls++;
    caps->input_delay = 99;      /* re-clamped after the hook */
    caps->ext.bytes[0] = 7;      /* the engine's own byte survives */
}

static void case_default_caps(void)
{
    RecompNetplayHostHooks h;
    RNetLobbyMatchCaps caps;
    printf("  default caps\n");
    memset(&h, 0, sizeof(h));
    h.game_name = "Test Game";
    h.max_players = 4;
    h.fill_match_caps = fill_hook;
    ck(recomp_netplay_host_init(&h) == 0, "init");
    caps = default_caps(NULL);
    ck(fill_calls == 1, "the engine's hook ran");
    ck(caps.valid && caps.rollback == 1, "rollback is the lobby default");
    ck(caps.input_delay == 20, "the hook's delay is clamped to 20");
    ck(caps.input_prediction == 0, "no runway until somebody sets one");
    ck(caps.ext.bytes[0] == 7, "ext is the engine's");
    ck(caps.mod_count == 0 && caps.mod_set[0] == '\0',
       "a vanilla build publishes an empty plan");

    ck(cb_rollback_set(NULL, 0) == 0, "rollback off before a room: pending");
    ck(cb_input_prediction_set(NULL, 40) == 0, "a runway before a room");
    caps = default_caps(NULL);
    ck(caps.rollback == 0, "the next create publishes rollback off");
    ck(caps.input_prediction == 16, "and the runway, clamped to 16");
    ck(cb_rollback_get(NULL) == 0 && cb_input_prediction_get(NULL) == 16,
       "and reads them back");
}

static void case_seat_ceiling(void)
{
    printf("  seat ceiling\n");
    hooks_with(RECOMP_NETPLAY_SLOTS_HOST_FIRST, 4);
    ck(clamp_lobby_max_slots(8) == 4, "an N64-style title caps at 4");
    ck(clamp_lobby_max_slots(1) == 2, "and never below 2");
    hooks_with(RECOMP_NETPLAY_SLOTS_SEAT, 0);
    ck(clamp_lobby_max_slots(4) == 2, "an unset ceiling is two seats");
}

static void case_table_and_names(void)
{
    const RecompLauncherCNetplayCallbacks *cb;
    RecompNetplayHostHooks h;
    printf("  callback table\n");
    memset(&h, 0, sizeof(h));
    ck(recomp_netplay_host_init(&h) == -1, "init refuses a nameless title");
    hooks_with(RECOMP_NETPLAY_SLOTS_HOST_FIRST, 4);
    cb = recomp_netplay_host_callbacks();
    ck(cb != NULL, "a table after init");
    ck(cb && cb->rollback_get && cb->rollback_set && cb->input_prediction_get &&
       cb->input_prediction_set && cb->connecting && cb->name_rejected &&
       cb->need_mods_count && cb->need_mods_get && cb->need_mods_can_transfer &&
       cb->mod_xfer_progress,
       "the callbacks the SNES backend left NULL are wired");
    ck(cb && cb->host_can_spectate == NULL,
       "host_can_spectate stays NULL until an engine is measured doing it");
    ck(!strcmp(rnet_lobby_game_version(), "1.2.3"), "the pin reached the client");
    ck(cb_name_rejected(NULL, "fuck off") == 1, "a filtered name is refused");
    ck(cb_name_rejected(NULL, "Marisa") == 0, "an ordinary name is not");
    ck(cb_connecting(NULL) == 0, "not connecting while disconnected");
    ck(cb_need_mods_can_transfer(NULL) == 0,
       "no pre-seat transfer is offered (the transfer needs a seat)");
}

int main(void)
{
    case_host_first_standard_and_swapped();
    case_host_first_sparse_four();
    case_host_first_host_in_gallery();
    case_seat_policy();
    case_lan_launch();
    case_default_caps();
    case_seat_ceiling();
    case_table_and_names();
    printf(fails ? "\n%d failure(s)\n" : "\nall netplay host cases passed\n",
           fails);
    return fails != 0;
}
