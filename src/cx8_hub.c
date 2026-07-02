/*
 * cx8_hub.c — Chromaplex 8 Community Cart Hub
 *
 * Provides cartridge sharing via a simple HTTP REST API.
 * Uses raw sockets (WinSock on Windows, POSIX on Unix) to avoid
 * external dependencies like libcurl.
 *
 * For now, implements a local hub cache and stubbed networking
 * that can be activated when a community server is available.
 */

#include "cx8_hub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

/* ─── State ────────────────────────────────────────────────── */

static char s_hub_url[256] = "https://hub.chromaplex8.io";
static char s_carts_dir[512] = "";
static char s_error[256] = "";
static cx8_hub_state_t s_state = CX8_HUB_IDLE;

static cx8_hub_entry_t s_entries[CX8_HUB_PAGE_SIZE];
static int s_entry_count = 0;

/* ─── Minimal HTTP GET (no TLS — for plain HTTP hub or local) ─── */

static bool http_get(const char *host, int port, const char *path,
                     char *response, size_t max_response)
{
    SOCKET sock;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        snprintf(s_error, sizeof(s_error), "DNS lookup failed: %s", host);
        return false;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(res);
        snprintf(s_error, sizeof(s_error), "Socket creation failed");
        return false;
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        closesocket(sock);
        freeaddrinfo(res);
        snprintf(s_error, sizeof(s_error), "Connection failed: %s:%d", host, port);
        return false;
    }
    freeaddrinfo(res);

    /* Send HTTP request */
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: Chromaplex8/1.0\r\n"
             "Accept: application/json\r\n"
             "\r\n",
             path, host);

    send(sock, request, (int)strlen(request), 0);

    /* Read response */
    size_t total = 0;
    int received;
    while (total < max_response - 1) {
        received = recv(sock, response + total, (int)(max_response - 1 - total), 0);
        if (received <= 0) break;
        total += (size_t)received;
    }
    response[total] = '\0';

    closesocket(sock);
    return total > 0;
}

/* ─── Simple JSON parser (minimal — extracts hub entries) ───── */

static const char *find_json_value(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    return p;
}

static int extract_json_string(const char *p, char *out, size_t max)
{
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < max - 1) {
        if (*p == '\\') p++;  /* skip escape */
        out[i++] = *p++;
    }
    out[i] = '\0';
    return (int)i;
}

/* ─── Public API ──────────────────────────────────────────── */

void cx8_hub_init(const char *carts_dir)
{
    if (carts_dir)
        strncpy(s_carts_dir, carts_dir, sizeof(s_carts_dir) - 1);
    s_state = CX8_HUB_IDLE;
    s_entry_count = 0;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    printf("[CX8-HUB] Community hub ready (%s)\n", s_hub_url);
}

void cx8_hub_shutdown(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void cx8_hub_set_url(const char *url)
{
    if (url) strncpy(s_hub_url, url, sizeof(s_hub_url) - 1);
}

cx8_hub_state_t cx8_hub_get_state(void)
{
    return s_state;
}

void cx8_hub_fetch_list(int page)
{
    s_state = CX8_HUB_FETCHING;
    s_entry_count = 0;

    /* Parse host from URL */
    char host[128] = "";
    int port = 80;
    const char *url = s_hub_url;

    /* Skip protocol */
    if (strncmp(url, "https://", 8) == 0) { url += 8; port = 443; }
    else if (strncmp(url, "http://", 7) == 0) { url += 7; port = 80; }

    /* Extract host */
    const char *slash = strchr(url, '/');
    const char *colon = strchr(url, ':');
    if (colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - url);
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, url, hlen);
        host[hlen] = '\0';
        port = atoi(colon + 1);
    } else if (slash) {
        size_t hlen = (size_t)(slash - url);
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, url, hlen);
        host[hlen] = '\0';
    } else {
        size_t hlen = strlen(url);
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, url, hlen);
        host[hlen] = '\0';
    }

    /* Build path */
    char path[256];
    snprintf(path, sizeof(path), "/api/carts?page=%d&limit=%d", page, CX8_HUB_PAGE_SIZE);

    /* Attempt fetch (HTTPS not supported without TLS lib — treat as offline) */
    if (port == 443) {
        s_state = CX8_HUB_OFFLINE;
        snprintf(s_error, sizeof(s_error),
                 "HTTPS not yet supported — configure HTTP hub URL or use local carts");
        printf("[CX8-HUB] Offline (HTTPS requires TLS library)\n");
        return;
    }

    char response[32768];
    if (!http_get(host, port, path, response, sizeof(response))) {
        s_state = CX8_HUB_OFFLINE;
        printf("[CX8-HUB] Offline: %s\n", s_error);
        return;
    }

    /* Skip HTTP headers — find body after \r\n\r\n */
    char *body = strstr(response, "\r\n\r\n");
    if (!body) {
        s_state = CX8_HUB_ERROR;
        snprintf(s_error, sizeof(s_error), "Invalid HTTP response");
        return;
    }
    body += 4;

    /* Parse simple JSON array of cart objects */
    const char *p = body;
    while (*p && s_entry_count < CX8_HUB_PAGE_SIZE) {
        const char *obj = strchr(p, '{');
        if (!obj) break;

        cx8_hub_entry_t *e = &s_entries[s_entry_count];
        memset(e, 0, sizeof(*e));

        const char *v;
        if ((v = find_json_value(obj, "id")))
            extract_json_string(v, e->id, sizeof(e->id));
        if ((v = find_json_value(obj, "title")))
            extract_json_string(v, e->title, sizeof(e->title));
        if ((v = find_json_value(obj, "author")))
            extract_json_string(v, e->author, sizeof(e->author));
        if ((v = find_json_value(obj, "description")))
            extract_json_string(v, e->description, sizeof(e->description));

        if (e->id[0]) s_entry_count++;

        /* Move past this object */
        const char *close = strchr(obj, '}');
        if (!close) break;
        p = close + 1;
    }

    s_state = CX8_HUB_READY;
    printf("[CX8-HUB] Fetched %d carts\n", s_entry_count);
}

int cx8_hub_entry_count(void)
{
    return s_entry_count;
}

const cx8_hub_entry_t *cx8_hub_entry(int index)
{
    if (index < 0 || index >= s_entry_count) return NULL;
    return &s_entries[index];
}

const char *cx8_hub_download(const char *cart_id)
{
    if (!cart_id || !s_carts_dir[0]) {
        snprintf(s_error, sizeof(s_error), "No cart ID or carts directory");
        return NULL;
    }

    /* Parse host from URL (same as fetch_list) */
    char host[128] = "";
    int port = 80;
    const char *url = s_hub_url;
    if (strncmp(url, "https://", 8) == 0) { url += 8; port = 443; }
    else if (strncmp(url, "http://", 7) == 0) { url += 7; port = 80; }

    const char *slash = strchr(url, '/');
    if (slash) {
        size_t hlen = (size_t)(slash - url);
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, url, hlen);
        host[hlen] = '\0';
    } else {
        size_t hlen = strlen(url);
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, url, hlen);
        host[hlen] = '\0';
    }

    if (port == 443) {
        snprintf(s_error, sizeof(s_error), "HTTPS download not supported");
        return NULL;
    }

    char path[256];
    snprintf(path, sizeof(path), "/api/carts/%s", cart_id);

    char response[65536];
    if (!http_get(host, port, path, response, sizeof(response))) {
        return NULL;
    }

    /* Find body */
    char *body = strstr(response, "\r\n\r\n");
    if (!body) return NULL;
    body += 4;

    /* Save to carts directory */
    static char save_path[512];
    snprintf(save_path, sizeof(save_path), "%s/%s.lua", s_carts_dir, cart_id);

    FILE *f = fopen(save_path, "wb");
    if (!f) {
        snprintf(s_error, sizeof(s_error), "Failed to save: %s", save_path);
        return NULL;
    }
    fwrite(body, 1, strlen(body), f);
    fclose(f);

    printf("[CX8-HUB] Downloaded: %s → %s\n", cart_id, save_path);
    return save_path;
}

bool cx8_hub_upload(const char *path)
{
    /* Upload requires POST — stubbed for future implementation */
    (void)path;
    snprintf(s_error, sizeof(s_error), "Upload not yet implemented — coming soon!");
    printf("[CX8-HUB] Upload not yet available\n");
    return false;
}

const char *cx8_hub_error(void)
{
    return s_error;
}
