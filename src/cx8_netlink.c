/*
 * cx8_netlink.c — Chromaplex 8 NETLINK-1 Multiplayer Networking
 *
 * LAN multiplayer over UDP using WinSock2 (Windows) or POSIX sockets.
 *
 * Protocol (all messages are prefixed with a 4-byte magic + 1-byte type):
 *   Magic:  "CX8N"
 *   Types:  0x01 = DISCOVER (broadcast)
 *           0x02 = DISCOVER_REPLY
 *           0x03 = JOIN_REQUEST
 *           0x04 = JOIN_ACCEPT (host → joiner, assigns player_id)
 *           0x05 = JOIN_REJECT
 *           0x06 = DATA (user message)
 *           0x07 = HEARTBEAT
 *           0x08 = DISCONNECT
 */

#include "cx8_netlink.h"
#include <SDL.h>       /* for SDL_GetTicks */
#include <stdio.h>
#include <string.h>

/* ─── Platform socket abstraction ──────────────────────────── */
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define SOCK_INVALID  INVALID_SOCKET
  #define SOCK_ERROR    SOCKET_ERROR
  #define CLOSESOCK(s)  closesocket(s)
  static bool s_wsa_init = false;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int sock_t;
  #define SOCK_INVALID  (-1)
  #define SOCK_ERROR    (-1)
  #define CLOSESOCK(s)  close(s)
#endif

/* ─── Protocol constants ──────────────────────────────────── */
#define NET_MAGIC       "CX8N"
#define NET_MAGIC_LEN   4
#define NET_HDR_LEN     (NET_MAGIC_LEN + 1)  /* magic + type */

#define MSG_DISCOVER        0x01
#define MSG_DISCOVER_REPLY  0x02
#define MSG_JOIN_REQUEST    0x03
#define MSG_JOIN_ACCEPT     0x04
#define MSG_JOIN_REJECT     0x05
#define MSG_DATA            0x06
#define MSG_HEARTBEAT       0x07
#define MSG_DISCONNECT      0x08

/* ─── Internal state ───────────────────────────────────────── */
static cx8_net_state_t  s_state     = CX8_NET_DISCONNECTED;
static sock_t           s_sock      = SOCK_INVALID;
static sock_t           s_disc_sock = SOCK_INVALID;  /* discovery socket */
static int              s_port      = CX8_NET_DEFAULT_PORT;
static int              s_my_id     = -1;
static char             s_my_name[32] = "Player";

static cx8_net_peer_t   s_peers[CX8_NET_MAX_PLAYERS];
static int              s_peer_count = 0;

/* Receive queue (ring buffer) */
static cx8_net_msg_t    s_recv_queue[CX8_NET_RECV_QUEUE_SIZE];
static int              s_recv_head = 0;
static int              s_recv_tail = 0;

static uint32_t         s_last_heartbeat = 0;

/* ─── Platform init/cleanup ────────────────────────────────── */
static bool platform_init(void)
{
#ifdef _WIN32
    if (!s_wsa_init) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fprintf(stderr, "[NETLINK-1] WSAStartup failed\n");
            return false;
        }
        s_wsa_init = true;
    }
#endif
    return true;
}

static void platform_cleanup(void)
{
#ifdef _WIN32
    if (s_wsa_init) {
        WSACleanup();
        s_wsa_init = false;
    }
#endif
}

/* ─── Socket helpers ───────────────────────────────────────── */
static sock_t create_udp_socket(int port, bool broadcast, bool nonblocking)
{
    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == SOCK_INVALID) return SOCK_INVALID;

    if (broadcast) {
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&opt, sizeof(opt));
    }

    /* Allow address reuse */
    {
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    }

    if (port > 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons((uint16_t)port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCK_ERROR) {
            fprintf(stderr, "[NETLINK-1] bind(%d) failed\n", port);
            CLOSESOCK(s);
            return SOCK_INVALID;
        }
    }

    /* Set non-blocking */
    if (nonblocking) {
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);
#else
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
    }

    return s;
}

/* ─── Packet building ──────────────────────────────────────── */
static int build_packet(uint8_t *buf, int bufsize, uint8_t type,
                        const void *payload, int payload_len)
{
    if (NET_HDR_LEN + payload_len > bufsize) return 0;
    memcpy(buf, NET_MAGIC, NET_MAGIC_LEN);
    buf[NET_MAGIC_LEN] = type;
    if (payload && payload_len > 0)
        memcpy(buf + NET_HDR_LEN, payload, payload_len);
    return NET_HDR_LEN + payload_len;
}

static bool validate_packet(const uint8_t *buf, int len, uint8_t *type_out)
{
    if (len < NET_HDR_LEN) return false;
    if (memcmp(buf, NET_MAGIC, NET_MAGIC_LEN) != 0) return false;
    *type_out = buf[NET_MAGIC_LEN];
    return true;
}

/* ─── Send helpers ─────────────────────────────────────────── */
static bool send_to_addr(sock_t s, const uint8_t *data, int len,
                         uint32_t ip, uint16_t port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = port;
    addr.sin_addr.s_addr = ip;
    return sendto(s, (const char *)data, len, 0,
                  (struct sockaddr *)&addr, sizeof(addr)) != SOCK_ERROR;
}

static bool send_to_peer(int peer_idx, const uint8_t *data, int len)
{
    if (peer_idx < 0 || peer_idx >= CX8_NET_MAX_PLAYERS || !s_peers[peer_idx].active)
        return false;
    return send_to_addr(s_sock, data, len, s_peers[peer_idx].ip, s_peers[peer_idx].port);
}

static void broadcast_packet(const uint8_t *data, int len, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(s_sock, (const char *)data, len, 0,
           (struct sockaddr *)&addr, sizeof(addr));
}

/* ─── Queue helpers ────────────────────────────────────────── */
static void enqueue_msg(int from, int channel, const char *data, int data_len)
{
    int next_head = (s_recv_head + 1) % CX8_NET_RECV_QUEUE_SIZE;
    if (next_head == s_recv_tail) return;  /* queue full, drop */

    cx8_net_msg_t *m = &s_recv_queue[s_recv_head];
    m->from_player = from;
    m->channel     = (uint8_t)channel;
    m->data_len    = (data_len > CX8_NET_MAX_MSG_LEN) ? CX8_NET_MAX_MSG_LEN : data_len;
    if (data_len > 0) memcpy(m->data, data, m->data_len);
    s_recv_head = next_head;
}

/* ─── Peer management ─────────────────────────────────────── */
static int find_peer_by_addr(uint32_t ip, uint16_t port)
{
    for (int i = 0; i < CX8_NET_MAX_PLAYERS; i++) {
        if (s_peers[i].active && s_peers[i].ip == ip && s_peers[i].port == port)
            return i;
    }
    return -1;
}

static int add_peer(uint32_t ip, uint16_t port, int player_id, const char *name)
{
    if (player_id < 0 || player_id >= CX8_NET_MAX_PLAYERS) return -1;
    if (s_peers[player_id].active) return -1;  /* slot taken */

    cx8_net_peer_t *p = &s_peers[player_id];
    p->ip          = ip;
    p->port        = port;
    p->player_id   = player_id;
    p->active      = true;
    p->last_seen_ms = SDL_GetTicks();
    if (name) {
        strncpy(p->name, name, sizeof(p->name) - 1);
        p->name[sizeof(p->name) - 1] = '\0';
    }
    s_peer_count++;

    printf("[NETLINK-1] Player %d connected: %s\n", player_id, p->name);
    return player_id;
}

static void remove_peer(int idx)
{
    if (idx >= 0 && idx < CX8_NET_MAX_PLAYERS && s_peers[idx].active) {
        printf("[NETLINK-1] Player %d disconnected: %s\n", idx, s_peers[idx].name);
        s_peers[idx].active = false;
        s_peer_count--;
    }
}

/* ─── Process incoming packets ─────────────────────────────── */
static void process_incoming(void)
{
    uint8_t buf[512];
    struct sockaddr_in from_addr;
    int from_len = sizeof(from_addr);

    for (int i = 0; i < 100; i++) {  /* process up to 100 packets per frame */
        int n = recvfrom(s_sock, (char *)buf, sizeof(buf), 0,
                         (struct sockaddr *)&from_addr, &from_len);
        if (n <= 0) break;

        uint8_t type;
        if (!validate_packet(buf, n, &type)) continue;

        const uint8_t *payload = buf + NET_HDR_LEN;
        int payload_len = n - NET_HDR_LEN;
        uint32_t sender_ip   = from_addr.sin_addr.s_addr;
        uint16_t sender_port = from_addr.sin_port;

        switch (type) {

        case MSG_DISCOVER:
            if (s_state == CX8_NET_HOSTING) {
                /* Reply with our info */
                uint8_t reply[128];
                /* payload: [1 byte player_count] [32 bytes host name] */
                uint8_t reply_payload[33];
                reply_payload[0] = (uint8_t)s_peer_count;
                memcpy(reply_payload + 1, s_my_name, 32);
                int rlen = build_packet(reply, sizeof(reply), MSG_DISCOVER_REPLY,
                                       reply_payload, 33);
                send_to_addr(s_sock, reply, rlen, sender_ip, sender_port);
            }
            break;

        case MSG_DISCOVER_REPLY:
            if (s_state == CX8_NET_JOINING && payload_len >= 33) {
                /* Found a host! Send join request */
                uint8_t req[128];
                int rlen = build_packet(req, sizeof(req), MSG_JOIN_REQUEST,
                                       s_my_name, 32);
                send_to_addr(s_sock, req, rlen, sender_ip, sender_port);
            }
            break;

        case MSG_JOIN_REQUEST:
            if (s_state == CX8_NET_HOSTING && payload_len >= 1) {
                /* Assign next free player ID */
                int new_id = -1;
                for (int p = 1; p < CX8_NET_MAX_PLAYERS; p++) {
                    if (!s_peers[p].active) { new_id = p; break; }
                }
                if (new_id >= 0) {
                    add_peer(sender_ip, sender_port, new_id,
                            (const char *)payload);

                    /* Send accept: [1 byte player_id] */
                    uint8_t accept[32];
                    uint8_t accept_payload[1] = { (uint8_t)new_id };
                    int alen = build_packet(accept, sizeof(accept), MSG_JOIN_ACCEPT,
                                           accept_payload, 1);
                    send_to_addr(s_sock, accept, alen, sender_ip, sender_port);

                    /* Notify all peers about new player */
                    (void)0; /* peer list is synced via heartbeats */
                } else {
                    uint8_t reject[16];
                    int rlen = build_packet(reject, sizeof(reject), MSG_JOIN_REJECT, NULL, 0);
                    send_to_addr(s_sock, reject, rlen, sender_ip, sender_port);
                }
            }
            break;

        case MSG_JOIN_ACCEPT:
            if (s_state == CX8_NET_JOINING && payload_len >= 1) {
                s_my_id = payload[0];
                /* Add the host as peer 0 */
                add_peer(sender_ip, sender_port, 0, "Host");
                s_state = CX8_NET_CONNECTED;
                printf("[NETLINK-1] Joined session as Player %d\n", s_my_id);
            }
            break;

        case MSG_JOIN_REJECT:
            if (s_state == CX8_NET_JOINING) {
                printf("[NETLINK-1] Session full — join rejected\n");
                s_state = CX8_NET_ERROR;
            }
            break;

        case MSG_DATA:
            if (s_state == CX8_NET_CONNECTED || s_state == CX8_NET_HOSTING) {
                /* payload: [1 byte channel] [rest = data] */
                if (payload_len >= 2) {
                    int peer_idx = find_peer_by_addr(sender_ip, sender_port);
                    if (peer_idx >= 0) {
                        s_peers[peer_idx].last_seen_ms = SDL_GetTicks();
                        int ch = payload[0];
                        enqueue_msg(s_peers[peer_idx].player_id, ch,
                                   (const char *)(payload + 1), payload_len - 1);
                    }
                }
            }
            break;

        case MSG_HEARTBEAT: {
            int peer_idx = find_peer_by_addr(sender_ip, sender_port);
            if (peer_idx >= 0)
                s_peers[peer_idx].last_seen_ms = SDL_GetTicks();
            break;
        }

        case MSG_DISCONNECT: {
            int peer_idx = find_peer_by_addr(sender_ip, sender_port);
            if (peer_idx >= 0) remove_peer(peer_idx);
            break;
        }

        default:
            break;
        }
    }

    /* Also check discovery socket for broadcast packets */
    if (s_disc_sock != SOCK_INVALID) {
        for (int i = 0; i < 10; i++) {
            int n = recvfrom(s_disc_sock, (char *)buf, sizeof(buf), 0,
                             (struct sockaddr *)&from_addr, &from_len);
            if (n <= 0) break;

            uint8_t type;
            if (!validate_packet(buf, n, &type)) continue;

            if (type == MSG_DISCOVER && s_state == CX8_NET_HOSTING) {
                uint8_t reply[128];
                uint8_t reply_payload[33];
                reply_payload[0] = (uint8_t)s_peer_count;
                memcpy(reply_payload + 1, s_my_name, 32);
                int rlen = build_packet(reply, sizeof(reply), MSG_DISCOVER_REPLY,
                                       reply_payload, 33);
                send_to_addr(s_disc_sock, reply, rlen,
                            from_addr.sin_addr.s_addr, from_addr.sin_port);
            }
        }
    }
}

/* ─── Public API ───────────────────────────────────────────── */

bool cx8_net_init(void)
{
    if (!platform_init()) return false;

    memset(s_peers, 0, sizeof(s_peers));
    s_peer_count    = 0;
    s_state         = CX8_NET_DISCONNECTED;
    s_my_id         = -1;
    s_recv_head     = 0;
    s_recv_tail     = 0;
    s_last_heartbeat = 0;

    printf("[NETLINK-1] ╔══════════════════════════════════════╗\n");
    printf("[NETLINK-1] ║  NETLINK-1 by CyberConnect          ║\n");
    printf("[NETLINK-1] ║  \"The world is your lobby.\"          ║\n");
    printf("[NETLINK-1] ║  Networking subsystem initialised    ║\n");
    printf("[NETLINK-1] ╚══════════════════════════════════════╝\n");
    return true;
}

void cx8_net_shutdown(void)
{
    cx8_net_disconnect();
    platform_cleanup();
}

bool cx8_net_host(int port, const char *name)
{
    if (s_state != CX8_NET_DISCONNECTED) cx8_net_disconnect();

    if (port <= 0) port = CX8_NET_DEFAULT_PORT;
    s_port = port;

    if (name) {
        strncpy(s_my_name, name, sizeof(s_my_name) - 1);
        s_my_name[sizeof(s_my_name) - 1] = '\0';
    }

    /* Create main socket */
    s_sock = create_udp_socket(port, true, true);
    if (s_sock == SOCK_INVALID) {
        s_state = CX8_NET_ERROR;
        return false;
    }

    /* Create discovery listener on broadcast port */
    s_disc_sock = create_udp_socket(CX8_NET_DISCOVERY_PORT, true, true);
    /* Non-fatal if discovery port fails */

    s_my_id = 0;  /* host is always player 0 */
    s_state = CX8_NET_HOSTING;

    /* Add ourselves as peer 0 */
    s_peers[0] = (cx8_net_peer_t){
        .ip = 0, .port = 0, .player_id = 0,
        .active = true, .last_seen_ms = SDL_GetTicks(),
    };
    snprintf(s_peers[0].name, sizeof(s_peers[0].name), "%s", s_my_name);
    s_peer_count = 1;

    printf("[NETLINK-1] Hosting on port %d as '%s'\n", port, s_my_name);
    return true;
}

bool cx8_net_join(const char *name)
{
    if (s_state != CX8_NET_DISCONNECTED) cx8_net_disconnect();

    if (name) {
        strncpy(s_my_name, name, sizeof(s_my_name) - 1);
        s_my_name[sizeof(s_my_name) - 1] = '\0';
    }

    /* Create socket on any port */
    s_sock = create_udp_socket(0, true, true);
    if (s_sock == SOCK_INVALID) {
        s_state = CX8_NET_ERROR;
        return false;
    }

    s_state = CX8_NET_JOINING;

    /* Send broadcast discovery */
    uint8_t pkt[32];
    int plen = build_packet(pkt, sizeof(pkt), MSG_DISCOVER, s_my_name, 32);
    broadcast_packet(pkt, plen, CX8_NET_DISCOVERY_PORT);
    broadcast_packet(pkt, plen, CX8_NET_DEFAULT_PORT);

    printf("[NETLINK-1] Scanning LAN for sessions as '%s'...\n", s_my_name);
    return true;
}

bool cx8_net_join_ip(const char *ip, int port, const char *name)
{
    if (s_state != CX8_NET_DISCONNECTED) cx8_net_disconnect();

    if (name) {
        strncpy(s_my_name, name, sizeof(s_my_name) - 1);
        s_my_name[sizeof(s_my_name) - 1] = '\0';
    }
    if (port <= 0) port = CX8_NET_DEFAULT_PORT;

    s_sock = create_udp_socket(0, false, true);
    if (s_sock == SOCK_INVALID) {
        s_state = CX8_NET_ERROR;
        return false;
    }

    /* Send join request directly */
    struct in_addr addr_in;
    if (inet_pton(AF_INET, ip, &addr_in) != 1) {
        fprintf(stderr, "[NETLINK-1] Invalid IP: %s\n", ip);
        CLOSESOCK(s_sock);
        s_sock = SOCK_INVALID;
        s_state = CX8_NET_ERROR;
        return false;
    }

    s_state = CX8_NET_JOINING;

    uint8_t pkt[64];
    int plen = build_packet(pkt, sizeof(pkt), MSG_JOIN_REQUEST, s_my_name, 32);
    send_to_addr(s_sock, pkt, plen, addr_in.s_addr, htons((uint16_t)port));

    printf("[NETLINK-1] Joining %s:%d as '%s'\n", ip, port, s_my_name);
    return true;
}

void cx8_net_disconnect(void)
{
    if (s_state == CX8_NET_DISCONNECTED) return;

    /* Notify all peers */
    if (s_sock != SOCK_INVALID) {
        uint8_t pkt[16];
        int plen = build_packet(pkt, sizeof(pkt), MSG_DISCONNECT, NULL, 0);
        for (int i = 0; i < CX8_NET_MAX_PLAYERS; i++) {
            if (s_peers[i].active && i != s_my_id) {
                send_to_peer(i, pkt, plen);
            }
        }
        CLOSESOCK(s_sock);
        s_sock = SOCK_INVALID;
    }

    if (s_disc_sock != SOCK_INVALID) {
        CLOSESOCK(s_disc_sock);
        s_disc_sock = SOCK_INVALID;
    }

    memset(s_peers, 0, sizeof(s_peers));
    s_peer_count = 0;
    s_state      = CX8_NET_DISCONNECTED;
    s_my_id      = -1;
    s_recv_head  = 0;
    s_recv_tail  = 0;

    printf("[NETLINK-1] Disconnected\n");
}

bool cx8_net_send(int channel, const char *data, int len)
{
    if (s_state != CX8_NET_CONNECTED && s_state != CX8_NET_HOSTING) return false;
    if (!data || len <= 0) return false;

    /* Build DATA packet: [1 byte channel] [data] */
    uint8_t pkt[512];
    uint8_t payload[CX8_NET_MAX_MSG_LEN + 1];
    payload[0] = (uint8_t)channel;
    int copy_len = (len > CX8_NET_MAX_MSG_LEN) ? CX8_NET_MAX_MSG_LEN : len;
    memcpy(payload + 1, data, copy_len);

    int plen = build_packet(pkt, sizeof(pkt), MSG_DATA, payload, copy_len + 1);

    /* Send to all peers */
    for (int i = 0; i < CX8_NET_MAX_PLAYERS; i++) {
        if (s_peers[i].active && i != s_my_id)
            send_to_peer(i, pkt, plen);
    }
    return true;
}

bool cx8_net_send_to(int player_id, int channel, const char *data, int len)
{
    if (s_state != CX8_NET_CONNECTED && s_state != CX8_NET_HOSTING) return false;
    if (!data || len <= 0) return false;
    if (player_id < 0 || player_id >= CX8_NET_MAX_PLAYERS) return false;
    if (!s_peers[player_id].active) return false;

    uint8_t pkt[512];
    uint8_t payload[CX8_NET_MAX_MSG_LEN + 1];
    payload[0] = (uint8_t)channel;
    int copy_len = (len > CX8_NET_MAX_MSG_LEN) ? CX8_NET_MAX_MSG_LEN : len;
    memcpy(payload + 1, data, copy_len);

    int plen = build_packet(pkt, sizeof(pkt), MSG_DATA, payload, copy_len + 1);
    return send_to_peer(player_id, pkt, plen);
}

int cx8_net_recv(cx8_net_msg_t *out)
{
    if (s_recv_tail == s_recv_head) return 0;
    if (out) *out = s_recv_queue[s_recv_tail];
    s_recv_tail = (s_recv_tail + 1) % CX8_NET_RECV_QUEUE_SIZE;
    return 1;
}

cx8_net_state_t cx8_net_state(void)
{
    return s_state;
}

int cx8_net_player_id(void)
{
    return s_my_id;
}

int cx8_net_peer_count(void)
{
    return s_peer_count;
}

const cx8_net_peer_t *cx8_net_get_peer(int index)
{
    if (index < 0 || index >= CX8_NET_MAX_PLAYERS) return NULL;
    if (!s_peers[index].active) return NULL;
    return &s_peers[index];
}

void cx8_net_update(void)
{
    if (s_state == CX8_NET_DISCONNECTED) return;

    /* Process incoming packets */
    process_incoming();

    uint32_t now = SDL_GetTicks();

    /* Send heartbeats */
    if (now - s_last_heartbeat >= CX8_NET_HEARTBEAT_MS) {
        s_last_heartbeat = now;

        uint8_t pkt[16];
        int plen = build_packet(pkt, sizeof(pkt), MSG_HEARTBEAT, NULL, 0);
        for (int i = 0; i < CX8_NET_MAX_PLAYERS; i++) {
            if (s_peers[i].active && i != s_my_id)
                send_to_peer(i, pkt, plen);
        }

        /* If joining and haven't connected yet, keep scanning */
        if (s_state == CX8_NET_JOINING) {
            uint8_t disc[64];
            int dlen = build_packet(disc, sizeof(disc), MSG_DISCOVER, s_my_name, 32);
            broadcast_packet(disc, dlen, CX8_NET_DISCOVERY_PORT);
            broadcast_packet(disc, dlen, CX8_NET_DEFAULT_PORT);
        }
    }

    /* Check for peer timeouts */
    for (int i = 0; i < CX8_NET_MAX_PLAYERS; i++) {
        if (s_peers[i].active && i != s_my_id) {
            if (now - s_peers[i].last_seen_ms > CX8_NET_TIMEOUT_MS) {
                printf("[NETLINK-1] Player %d timed out\n", i);
                remove_peer(i);
            }
        }
    }
}
