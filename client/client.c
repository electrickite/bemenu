#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <bemenu.h>

static struct {
    enum bm_filter_mode filter_mode;
    int wrap;
    unsigned int lines;
    const char *title;
    int selected;
    int bottom;
    int grab;
    int monitor;
} client = {
    .filter_mode = BM_FILTER_MODE_DMENU,
    .wrap = 0,
    .lines = 0,
    .title = "bemenu",
    .selected = 0,
    .bottom = 0,
    .grab = 0,
    .monitor = 0
};

static void
disco_trap(int sig)
{
    (void)sig;
    printf("\e[?25h\n");
    fflush(stdout);
    exit(EXIT_FAILURE);
}

static void
disco(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = disco_trap;
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGTRAP, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    unsigned int i, cc, c = 80;
    printf("\e[?25l");
    while (1) {
        for (i = 1; i < c - 1; ++i) {
            printf("\r    %*s%s %s %s ", (i > c / 2 ? c - i : i), " ", ((i % 2) ? "<o/" : "\\o>"), ((i % 4) ? "DISCO" : "     "), ((i %2) ? "\\o>" : "<o/"));
            for (cc = 0; cc < (i < c / 2 ? c / 2 - i : i - c / 2); ++cc) printf(((i % 2) ? "^" : "'"));
            printf("%s %s \r %s %s", ((i % 2) ? "*" : "•"), ((i % 3) ? "\\o" : "<o"), ((i % 3) ? "o/" : "o>"), ((i % 2) ? "*" : "•"));
            for (cc = 2; cc < (i > c / 2 ? c - i : i); ++cc) printf(((i % 2) ? "^" : "'"));
            fflush(stdout);
            usleep(140 * 1000);
        }
    }
    printf("\e[?25h");
    exit(EXIT_SUCCESS);
}

static void
version(const char *name)
{
    char *base = strrchr(name, '/');
    printf("%s v%s\n", (base ? base  + 1 : name), bm_version());
    exit(EXIT_SUCCESS);
}

static void
usage(FILE *out, const char *name)
{
    char *base = strrchr(name, '/');
    fprintf(out, "usage: %s [options]\n", (base ? base + 1 : name));
    fputs("Options\n"
          " -h, --help            display this help and exit.\n"
          " -v, --version         display version.\n"
          " -i, --ignorecase      match items case insensitively.\n"
          " -w, --wrap            wraps cursor selection.\n"
          " -l, --list            list items vertically with the given number of lines.\n"
          " -p, --prompt          defines the prompt text to be displayed.\n"
          " -I, --index           select item at index automatically.\n\n"

          "Backend specific options\n"
          "   c = ncurses\n" // x == x11
          "   (...) At end of help indicates the backend support for option.\n\n"

          " -b, --bottom          appears at the bottom of the screen. ()\n"
          " -f, --grab            grabs the keyboard before reading stdin. ()\n"
          " -m, --monitor         index of monitor where menu will appear. ()\n"
          " --fn                  defines the font to be used. ()\n"
          " --nb                  defines the normal background color. ()\n"
          " --nf                  defines the normal foreground color. ()\n"
          " --sb                  defines the selected background color. ()\n"
          " --sf                  defines the selected foreground color. ()\n", out);
    exit((out == stderr ? EXIT_FAILURE : EXIT_SUCCESS));
}

static void
parse_args(int *argc, char **argv[])
{
    static const struct option opts[] = {
        { "help",        no_argument,       0, 'h' },
        { "version",     no_argument,       0, 'v' },

        { "ignorecase",  no_argument,       0, 'i' },
        { "wrap",        no_argument,       0, 'w' },
        { "list",        required_argument, 0, 'l' },
        { "prompt",      required_argument, 0, 'p' },
        { "index",       required_argument, 0, 'I' },

        { "bottom",      no_argument,       0, 'b' },
        { "grab",        no_argument,       0, 'f' },
        { "monitor",     required_argument, 0, 'm' },
        { "fn",          required_argument, 0, 0x100 },
        { "nb",          required_argument, 0, 0x101 },
        { "nf",          required_argument, 0, 0x102 },
        { "sb",          required_argument, 0, 0x103 },
        { "sf",          required_argument, 0, 0x104 },

        { "disco",       no_argument,       0, 0x105 },
        { 0, 0, 0, 0 }
    };

    /* TODO: getopt does not support -sf, -sb etc..
     * Either break the interface and make them --sf, --sb (like they are now),
     * or parse them before running getopt.. */

    for (;;) {
        int opt = getopt_long(*argc, *argv, "hviwl:I:p:I:bfm:", opts, NULL);
        if (opt < 0)
            break;

        switch (opt) {
            case 'h':
                usage(stdout, *argv[0]);
                break;
            case 'v':
                version(*argv[0]);
                break;

            case 'i':
                client.filter_mode = BM_FILTER_MODE_DMENU_CASE_INSENSITIVE;
                break;
            case 'w':
                client.wrap = 1;
                break;
            case 'l':
                client.lines = strtol(optarg, NULL, 10);
                break;
            case 'p':
                client.title = optarg;
                break;
            case 'I':
                client.selected = strtol(optarg, NULL, 10);
                break;

            case 'b':
                client.bottom = 1;
                break;
            case 'f':
                client.grab = 1;
                break;
            case 'm':
                client.monitor = strtol(optarg, NULL, 10);
                break;

            case 0x100:
            case 0x101:
            case 0x102:
            case 0x103:
            case 0x104:
                break;

            case 0x105:
                disco();
                break;

            case ':':
            case '?':
                fputs("\n", stderr);
                usage(stderr, *argv[0]);
                break;
        }
    }

    *argc -= optind;
    *argv += optind;
}

static void
read_items_to_menu_from_stdin(struct bm_menu *menu)
{
    assert(menu);

    size_t step = 1024, allocated;
    char *buffer;

    if (!(buffer = malloc((allocated = step))))
        return;

    size_t read;
    while ((read = fread(buffer + (allocated - step), 1, step, stdin)) == step) {
        void *tmp;
        if (!(tmp = realloc(buffer, (allocated += step)))) {
            free(buffer);
            return;
        }
        buffer = tmp;
    }
    buffer[allocated - step + read - 1] = 0;

    size_t pos;
    char *s = buffer;
    while ((pos = strcspn(s, "\n")) != 0) {
        size_t next = pos + (s[pos] != 0);
        s[pos] = 0;

        struct bm_item *item;
        if (!(item = bm_item_new(s)))
            break;

        bm_menu_add_item(menu, item);
        s += next;
    }

    free(buffer);
}

int
main(int argc, char **argv)
{
    if (!bm_init())
        return EXIT_FAILURE;

    parse_args(&argc, &argv);

    struct bm_menu *menu;
    if (!(menu = bm_menu_new(NULL)))
        return EXIT_FAILURE;

    bm_menu_set_title(menu, client.title);
    bm_menu_set_filter_mode(menu, client.filter_mode);
    bm_menu_set_wrap(menu, client.wrap);

    read_items_to_menu_from_stdin(menu);

    bm_menu_set_highlighted_index(menu, client.selected);

    enum bm_key key;
    unsigned int unicode;
    int status = 0;
    do {
        bm_menu_render(menu);
        key = bm_menu_poll_key(menu, &unicode);
    } while ((status = bm_menu_run_with_key(menu, key, unicode)) == BM_RUN_RESULT_RUNNING);

    if (status == BM_RUN_RESULT_SELECTED) {
        unsigned int i, count;
        struct bm_item **items = bm_menu_get_selected_items(menu, &count);
        for (i = 0; i < count; ++i) {
            const char *text = bm_item_get_text(items[i]);
            printf("%s\n", (text ? text : ""));
        }

        if (!count && bm_menu_get_filter(menu))
            printf("%s\n", bm_menu_get_filter(menu));
    }

    bm_menu_free(menu);
    return (status == BM_RUN_RESULT_SELECTED ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set ts=8 sw=4 tw=0 :*/
