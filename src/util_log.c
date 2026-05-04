#include "util_log.h"

#include <stdio.h>
#include <stdbool.h>
#include <zlog.h>

static zlog_category_t *g_log_cat = NULL;
static bool g_log_inited = false;

static void rm_log_vfallback(const char *level, const char *fmt, va_list ap)
{
    fprintf(stderr, "[%s] ", level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

int rm_log_init(const char *config_path)
{
    int rc;

    if (g_log_inited) {
        return 0;
    }

    rc = zlog_init(config_path);
    if (rc != 0) {
        fprintf(stderr, "Failed to init zlog with config %s\n", config_path);
        return -1;
    }

    g_log_cat = zlog_get_category("resource_manager");
    if (!g_log_cat) {
        zlog_fini();
        fprintf(stderr, "Failed to get zlog category resource_manager\n");
        return -1;
    }

    g_log_inited = true;
    return 0;
}

void rm_log_fini(void)
{
    if (!g_log_inited) {
        return;
    }

    zlog_fini();
    g_log_cat = NULL;
    g_log_inited = false;
}

void rm_log_info(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (g_log_cat) {
        vzlog_info(g_log_cat, fmt, ap);
    } else {
        rm_log_vfallback("INFO", fmt, ap);
    }
    va_end(ap);
}

void rm_log_warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (g_log_cat) {
        vzlog_warn(g_log_cat, fmt, ap);
    } else {
        rm_log_vfallback("WARN", fmt, ap);
    }
    va_end(ap);
}

void rm_log_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (g_log_cat) {
        vzlog_error(g_log_cat, fmt, ap);
    } else {
        rm_log_vfallback("ERROR", fmt, ap);
    }
    va_end(ap);
}

void rm_log_debug(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (g_log_cat) {
        vzlog_debug(g_log_cat, fmt, ap);
    } else {
        rm_log_vfallback("DEBUG", fmt, ap);
    }
    va_end(ap);
}
