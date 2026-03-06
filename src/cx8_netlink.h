/*
 * cx8_netlink.h — Chromaplex 8 NETLINK-1 Multiplayer Networking
 *
 * ┌──────────────────────────────────────────────────────────┐
 * │  NETLINK-1          CyberConnect                        │
 * │  "The world is your lobby."                             │
 * │  LAN multiplayer via UDP  ·  Antenna + blinking LEDs    │
 * └──────────────────────────────────────────────────────────┘
 *
 * Simple peer-to-peer LAN networking for up to 4 players.
 * Uses UDP with broadcast discovery and reliable messaging.
 *
 * Flow:
 *   1. Host calls net_host(port) to create a session
 *   2. Joiners call net_join() or net_join_ip(ip, port)
 *   3. All peers exchange messages with net_send() / net_recv()
 *   4. net_peers() returns a table of connected players
 *   5. net_disconnect() cleanly leaves the session
 */

#ifndef CX8_NETLINK_H
#define CX8_NETLINK_H

#include "cx8.h"

/* ─── Constants ────────────────────────────────────────────── */
#define CX8_NET_MAX_PLAYERS     4
#define CX8_NET_DEFAULT_PORT    7888
#define CX8_NET_MAX_MSG_LEN     256
#define CX8_NET_RECV_QUEUE_SIZE 64
#define CX8_NET_DISCOVERY_PORT  7889
#define CX8_NET_HEARTBEAT_MS    500
#define CX8_NET_TIMEOUT_MS      3000

/* ─── Network states ───────────────────────────────────────── */
typedef enum {
    CX8_NET_DISCONNECTED = 0,
    CX8_NET_HOSTING,           /* listening for joins          */
    CX8_NET_JOINING,           /* scanning / connecting        */
    CX8_NET_CONNECTED,         /* in session                   */
    CX8_NET_ERROR              /* something went wrong         */
} cx8_net_state_t;

/* ─── Peer info ────────────────────────────────────────────── */
typedef struct {
    uint32_t    ip;             /* IPv4 in network byte order  */
    uint16_t    port;           /* in network byte order       */
    int         player_id;      /* 0-3                         */
    bool        active;
    uint32_t    last_seen_ms;   /* SDL_GetTicks timestamp      */
    char        name[32];
} cx8_net_peer_t;

/* ─── Message ──────────────────────────────────────────────── */
typedef struct {
    int         from_player;    /* sender player_id            */
    uint8_t     channel;        /* user-defined channel 0-255  */
    char        data[CX8_NET_MAX_MSG_LEN];
    int         data_len;
} cx8_net_msg_t;

/* ─── Lifecycle ────────────────────────────────────────────── */
bool            cx8_net_init(void);
void            cx8_net_shutdown(void);

/* ─── Session management ───────────────────────────────────── */
bool            cx8_net_host(int port, const char *name);
bool            cx8_net_join(const char *name);           /* broadcast scan */
bool            cx8_net_join_ip(const char *ip, int port, const char *name);
void            cx8_net_disconnect(void);

/* ─── Messaging ────────────────────────────────────────────── */
bool            cx8_net_send(int channel, const char *data, int len);
bool            cx8_net_send_to(int player_id, int channel, const char *data, int len);
int             cx8_net_recv(cx8_net_msg_t *out);   /* returns 1 if msg available */

/* ─── State queries ────────────────────────────────────────── */
cx8_net_state_t cx8_net_state(void);
int             cx8_net_player_id(void);             /* my id (0=host) */
int             cx8_net_peer_count(void);
const cx8_net_peer_t *cx8_net_get_peer(int index);

/* ─── Must be called each frame (handles heartbeats, timeouts) */
void            cx8_net_update(void);

#endif /* CX8_NETLINK_H */
