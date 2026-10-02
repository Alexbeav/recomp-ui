/* The setup wizard's error text holds a host's whole sentence (PS1G-63).
 *
 * A setup host tells the player why a disc is refused: which disc image the
 * kit was made from, which file was selected, and what it may be. With the
 * host's prefix that sentence runs to about 480 bytes. The wizard's text and
 * the job buffer the host writes into held 255, so the sentence was cut in
 * the middle of the selected file's name or size, and its last sentence was
 * never shown.
 *
 * Includes the model translation unit to run the prepare job's own code: the
 * thread function that calls the host, and the poll that moves the host's
 * reason into the wizard.
 */
#include "launcher_model.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pulled in by the model TU; unrelated to setup. */
void launcher_binds_set_zapper(int a, int b);
void launcher_binds_set_zapper(int a, int b) { (void)a; (void)b; }

static int fails;

static void expect(int cond, const char* what) {
    if (cond) { printf("ok: %s\n", what); return; }
    fprintf(stderr, "FAIL: %s\n", what);
    ++fails;
}

/* The longest refusal of the kits of 2026-10-02, as the host hands it over. */
static const char kSentence[] =
    "Disc verification failed: This is not the disc image this kit was made from. "
    "The kit needs Lost World, The - Jurassic Park - Special Edition (USA) "
    "(data track 264,858,720 bytes). The selected file Lost World, The - Jurassic "
    "Park - Special Edition (USA) (Track 1).bin is 264,858,720 bytes, SHA-1 "
    "1fac6ab6b0d7d3d8f5e70b40bb9fae1d2a2287bc. It may be another pressing, revision "
    "or region of the game (a different disc), a damaged copy, or the right disc in "
    "a form setup cannot read.";

static size_t given_cap;
static int host_prepare(const char* source, char* out_path, size_t out_cap,
                        char* err_msg, size_t err_cap,
                        RecompLauncherCPrepareProgressFn on_progress, void* ctx) {
    (void)source; (void)out_path; (void)out_cap; (void)on_progress; (void)ctx;
    given_cap = err_cap;
    snprintf(err_msg, err_cap, "%s", kSentence);
    return 0;   /* refused */
}

int main(void) {
    LauncherModel* m = (LauncherModel*)calloc(1, sizeof(LauncherModel));
    if (!m) { fprintf(stderr, "FAIL: out of memory\n"); return 1; }

    expect(strlen(kSentence) > 255, "the sentence is longer than the old 255 bytes");
    expect(strlen(kSentence) < LNG_SETUP_ERROR_CAP, "the sentence fits the wizard's text");
    expect(sizeof(m->setup_error) == LNG_SETUP_ERROR_CAP, "the wizard's text has the named size");
    expect(sizeof(g_prep_job.err) >= sizeof(m->setup_error),
           "the job buffer the host writes into is no smaller than the wizard's text");

    /* The prepare job, as launcher_model_begin_prepare_disc sets it up, run
     * here on this thread: the host refuses with its sentence. */
    m->prepare_with_progress_cb = host_prepare;
    memset(&g_prep_job, 0, sizeof(g_prep_job));
    g_prep_job.m = m;
    g_prep_job.kind = PREP_JOB_PREPARE;
    safe_copy(g_prep_job.source, sizeof(g_prep_job.source), "wrong.cue");
    m->setup_preparing = true;
    (void)prep_thread_main(&g_prep_job);
    expect(given_cap == LNG_SETUP_ERROR_CAP, "the host is given the whole room");
    expect(g_prep_job.done && !g_prep_job.result, "the job ended as refused");

    /* The poll of the next frame moves the reason into the wizard. */
    launcher_model_poll_prepare_disc(m);
    expect(!m->setup_preparing, "the wizard is no longer busy");
    expect(strcmp(m->setup_error, kSentence) == 0, "the wizard holds the host's sentence whole");
    expect(strstr(m->setup_error, "a damaged copy, or the right disc in a form setup cannot read.") != NULL,
           "the last sentence is there");

    /* A host that writes more than the room is cut at the room, and ended. */
    {
        static char big[2 * LNG_SETUP_ERROR_CAP];
        memset(big, 'x', sizeof(big) - 1);
        safe_copy(m->setup_error, sizeof(m->setup_error), big);
        expect(strlen(m->setup_error) == LNG_SETUP_ERROR_CAP - 1, "a longer text is cut at the room");
    }

    free(m);
    if (fails) {
        fprintf(stderr, "launcher setup error room: %d check(s) failed\n", fails);
        return 1;
    }
    printf("launcher setup error room: ok\n");
    return 0;
}
