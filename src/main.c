#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(PLATFORM_ANDROID)
extern void app_dummy(void);
#endif

#define NET_PORT 42042
#define NET_MAGIC 0x4D455453u

#define SAVE_DIR "magic_world"
#define WORLD_SLOT_COUNT 3

#define PACKET_STATE 1u
#define PACKET_LOADOUT 2u

#define PROFILE_MAGIC 0x4D50524Fu
#define PROFILE_VERSION 1u

typedef enum { NET_DISABLED = 0, NET_HOST = 1, NET_CLIENT = 2 } NetMode;
typedef enum { APP_MAIN_MENU = 0, APP_MULTI_MENU = 1, APP_OTHERS_MENU = 2, APP_GAME = 3 } AppScreen;

typedef struct {
    uint32_t magic;
    uint32_t packetType;
    uint32_t seq;
    uint32_t senderToken;
    int senderMode;
} NetPacketHeader;

typedef struct {
    NetPacketHeader h;

    float x;
    float y;
    float z;
    float health;
    float aimX;
    float aimY;
    float animTime;

    int activeSlot;
    int castLayer;
    float chargeLevel;
    float lifespanLevel;

    int castEvent;
    float castTargetX;
    float castTargetY;
    float castChargeMult;
    float castLifeMult;

    int spawnNpcEvent;
    float npcSpawnX;
    float npcSpawnY;

    int reviveEvent;
    int restartEvent;
} NetStatePacket;

typedef struct {
    NetPacketHeader h;
    int slotIndex;
    int activeSlot;
    HotbarSlot slotData;
} NetLoadoutPacket;

typedef struct {
    bool active;
    struct sockaddr_in addr;
    uint32_t token;
    uint32_t lastSeq;
    int playerIndex;
    float lastSeenTime;
} NetPeer;

typedef struct {
    bool enabled;
    NetMode mode;
    int socketFd;

    uint32_t localToken;
    uint32_t seq;

    struct sockaddr_in hostAddr;
    bool hasHost;

    NetPeer peers[MAX_PLAYERS - 1];
} NetSession;

typedef struct {
    bool cast;
    Vector2 castTarget;
    float castChargeMult;
    float castLifeMult;

    bool spawnNpc;
    Vector2 npcSpawnPos;

    bool revive;
    bool restart;

    bool loadoutSync;
    int loadoutSlot;
    HotbarSlot loadoutData;
} LocalNetEvents;

typedef struct {
    uint32_t magic;
    uint32_t version;
    Player player;
} PlayerProfileBlob;

static Vector2 NormalizeSafe(Vector2 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len <= 0.0001f) return (Vector2){1.0f, 0.0f};
    return (Vector2){v.x / len, v.y / len};
}

typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;

    bool castHold;
    bool castPressed;
    bool castReleased;
    bool lifeHold;

    bool jumpPressed;
    bool pausePressed;
    bool craftPressed;
    bool layerPressed;
    bool menuPressed;

    bool hasAim;
    Vector2 aimVirtual;
} MobileControlsState;

#if defined(PLATFORM_ANDROID)
static Vector2 ScreenToVirtualPoint(Vector2 p, float scale, float offsetX, float offsetY) {
    Vector2 v = {
        (p.x - offsetX) / scale,
        (p.y - offsetY) / scale
    };
    v.x = fmaxf(0.0f, fminf(v.x, SCREEN_W));
    v.y = fmaxf(0.0f, fminf(v.y, SCREEN_H));
    return v;
}
#endif

static uint32_t HashBytesFNV1a(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        hash ^= (uint32_t)bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t HashHotbarSlot(const HotbarSlot *slot) {
    return HashBytesFNV1a(slot, sizeof(*slot));
}

static int ClampSlotIndex(int slot) {
    if (slot < 0) return 0;
    if (slot > 9) return 9;
    return slot;
}

static int ClampSpellFormValue(int form) {
    if (form < FORM_PROJECTILE) return FORM_PROJECTILE;
    if (form > FORM_BEAM) return FORM_BEAM;
    return form;
}

static void SanitizeSigilGraphData(SigilGraph *graph) {
    if (!graph) return;

    if (!graph->nodes[0].active) {
        InitDefaultSpellNode(&graph->nodes[0]);
        graph->nodes[0].active = true;
        graph->nodes[0].pos = (Vector2){0.0f, 0.0f};
    }
    graph->nodes[0].parentId = -1;

    for (int i = 1; i < MAX_NODES; i++) {
        if (!graph->nodes[i].active) {
            graph->nodes[i].parentId = -1;
            continue;
        }

        int parentId = graph->nodes[i].parentId;
        if (parentId < 0 || parentId >= MAX_NODES || parentId == i || !graph->nodes[parentId].active) {
            graph->nodes[i].parentId = 0;
        }

        if (graph->nodes[i].sizeMod <= 0.01f) graph->nodes[i].sizeMod = 1.0f;
        if (graph->nodes[i].rangeMod <= 0.01f) graph->nodes[i].rangeMod = 1.0f;
        if (graph->nodes[i].speedMod <= 0.01f) graph->nodes[i].speedMod = 1.0f;
        if (graph->nodes[i].toolRadius <= 0.1f) graph->nodes[i].toolRadius = 2.0f;
    }
}

static void SanitizeHotbarSlot(HotbarSlot *slot) {
    if (!slot) return;

    if (slot->type != ITEM_SPELL && slot->type != ITEM_NPC) slot->type = ITEM_SPELL;
    if (slot->type == ITEM_SPELL) {
        slot->spell.form = ClampSpellFormValue(slot->spell.form);
        SanitizeSigilGraphData(&slot->spell.graph);
    }
}

static void SanitizePlayerLoadout(Player *p) {
    if (!p) return;

    p->activeSlot = ClampSlotIndex(p->activeSlot);
    if (p->castLayer != LAYER_GROUND && p->castLayer != LAYER_AIR) p->castLayer = LAYER_GROUND;

    for (int i = 0; i < 10; i++) {
        SanitizeHotbarSlot(&p->hotbar[i]);
    }
}

static void SeedHotbarHashes(const Player *p, uint32_t hashes[10]) {
    for (int i = 0; i < 10; i++) hashes[i] = HashHotbarSlot(&p->hotbar[i]);
}

static void QueueLoadoutSyncEvent(Player *p, LocalNetEvents *ev, uint32_t hashes[10], bool *forceAll, int *cursor) {
    ev->loadoutSync = false;
    ev->loadoutSlot = 0;

    if (*forceAll) {
        int slot = ClampSlotIndex(*cursor);
        ev->loadoutSync = true;
        ev->loadoutSlot = slot;
        ev->loadoutData = p->hotbar[slot];
        hashes[slot] = HashHotbarSlot(&p->hotbar[slot]);

        (*cursor)++;
        if (*cursor >= 10) {
            *cursor = 0;
            *forceAll = false;
        }
        return;
    }

    for (int i = 0; i < 10; i++) {
        uint32_t h = HashHotbarSlot(&p->hotbar[i]);
        if (h == hashes[i]) continue;

        hashes[i] = h;
        ev->loadoutSync = true;
        ev->loadoutSlot = i;
        ev->loadoutData = p->hotbar[i];
        return;
    }
}

static void UpdateMobileControls(MobileControlsState *state, float scale, float offsetX, float offsetY) {
    memset(state, 0, sizeof(*state));

#if defined(PLATFORM_ANDROID)
    static bool wasCastDown = false;
    static bool wasPauseDown = false;
    static bool wasCraftDown = false;
    static bool wasLayerDown = false;
    static bool wasMenuDown = false;
    static bool wasJumpDown = false;

    Rectangle upRec = {72, 328, 56, 50};
    Rectangle downRec = {72, 390, 56, 50};
    Rectangle leftRec = {10, 360, 56, 50};
    Rectangle rightRec = {134, 360, 56, 50};

    Rectangle castRec = {688, 342, 104, 98};
    Rectangle lifeRec = {574, 342, 104, 98};
    Rectangle jumpRec = {690, 246, 100, 84};

    Rectangle pauseRec = {720, 10, 70, 34};
    Rectangle craftRec = {640, 10, 70, 34};
    Rectangle layerRec = {560, 10, 70, 34};
    Rectangle menuRec = {480, 10, 70, 34};
    Rectangle aimRec = {360, 110, 430, 220};

    bool castDown = false;
    bool pauseDown = false;
    bool craftDown = false;
    bool layerDown = false;
    bool menuDown = false;
    bool jumpDown = false;

    int touchCount = GetTouchPointCount();
    for (int i = 0; i < touchCount; i++) {
        Vector2 tp = ScreenToVirtualPoint(GetTouchPosition(i), scale, offsetX, offsetY);
        if (CheckCollisionPointRec(tp, upRec)) state->up = true;
        if (CheckCollisionPointRec(tp, downRec)) state->down = true;
        if (CheckCollisionPointRec(tp, leftRec)) state->left = true;
        if (CheckCollisionPointRec(tp, rightRec)) state->right = true;

        if (CheckCollisionPointRec(tp, castRec)) castDown = true;
        if (CheckCollisionPointRec(tp, lifeRec)) state->lifeHold = true;
        if (CheckCollisionPointRec(tp, jumpRec)) jumpDown = true;
        if (CheckCollisionPointRec(tp, pauseRec)) pauseDown = true;
        if (CheckCollisionPointRec(tp, craftRec)) craftDown = true;
        if (CheckCollisionPointRec(tp, layerRec)) layerDown = true;
        if (CheckCollisionPointRec(tp, menuRec)) menuDown = true;

        if (CheckCollisionPointRec(tp, aimRec)) {
            state->hasAim = true;
            state->aimVirtual = tp;
        }
    }

    state->castHold = castDown;
    state->castPressed = castDown && !wasCastDown;
    state->castReleased = !castDown && wasCastDown;

    state->pausePressed = pauseDown && !wasPauseDown;
    state->craftPressed = craftDown && !wasCraftDown;
    state->layerPressed = layerDown && !wasLayerDown;
    state->menuPressed = menuDown && !wasMenuDown;
    state->jumpPressed = jumpDown && !wasJumpDown;

    wasCastDown = castDown;
    wasPauseDown = pauseDown;
    wasCraftDown = craftDown;
    wasLayerDown = layerDown;
    wasMenuDown = menuDown;
    wasJumpDown = jumpDown;
#else
    (void)scale;
    (void)offsetX;
    (void)offsetY;
#endif
}

static void DrawMobileControlsOverlay(const MobileControlsState *state) {
#if defined(PLATFORM_ANDROID)
    Color idle = (Color){20, 20, 20, 110};
    Color active = (Color){90, 120, 160, 190};

    Rectangle upRec = {72, 328, 56, 50};
    Rectangle downRec = {72, 390, 56, 50};
    Rectangle leftRec = {10, 360, 56, 50};
    Rectangle rightRec = {134, 360, 56, 50};
    Rectangle castRec = {688, 342, 104, 98};
    Rectangle lifeRec = {574, 342, 104, 98};
    Rectangle jumpRec = {690, 246, 100, 84};

    DrawRectangleRounded(upRec, 0.25f, 5, state->up ? active : idle);
    DrawRectangleRounded(downRec, 0.25f, 5, state->down ? active : idle);
    DrawRectangleRounded(leftRec, 0.25f, 5, state->left ? active : idle);
    DrawRectangleRounded(rightRec, 0.25f, 5, state->right ? active : idle);
    DrawText("U", 95, 344, 18, RAYWHITE);
    DrawText("D", 95, 405, 18, RAYWHITE);
    DrawText("L", 31, 376, 18, RAYWHITE);
    DrawText("R", 154, 376, 18, RAYWHITE);

    DrawRectangleRounded(castRec, 0.35f, 6, state->castHold ? active : idle);
    DrawRectangleRounded(lifeRec, 0.35f, 6, state->lifeHold ? active : idle);
    DrawRectangleRounded(jumpRec, 0.35f, 6, state->jumpPressed ? active : idle);
    DrawText("CAST", 722, 380, 16, RAYWHITE);
    DrawText("LIFE", 610, 380, 16, RAYWHITE);
    DrawText("JUMP", 722, 280, 14, RAYWHITE);

    DrawRectangleRounded((Rectangle){720, 10, 70, 34}, 0.35f, 4, idle);
    DrawRectangleRounded((Rectangle){640, 10, 70, 34}, 0.35f, 4, idle);
    DrawRectangleRounded((Rectangle){560, 10, 70, 34}, 0.35f, 4, idle);
    DrawRectangleRounded((Rectangle){480, 10, 70, 34}, 0.35f, 4, idle);
    DrawText("PAUSE", 728, 21, 9, WHITE);
    DrawText("CRAFT", 649, 21, 9, WHITE);
    DrawText("LAYER", 569, 21, 9, WHITE);
    DrawText("MENU", 491, 21, 9, WHITE);
#else
    (void)state;
#endif
}

static int ClampWorldSlot(int worldSlot) {
    if (worldSlot < 0) return 0;
    if (worldSlot >= WORLD_SLOT_COUNT) return WORLD_SLOT_COUNT - 1;
    return worldSlot;
}

static void BuildWorldSavePath(int worldSlot, char *outPath, int capacity) {
    int slot = ClampWorldSlot(worldSlot) + 1;
    snprintf(outPath, (size_t)capacity, "%s/world_%d.bin", SAVE_DIR, slot);
}

static void BuildProfileSavePath(int worldSlot, char *outPath, int capacity) {
    int slot = ClampWorldSlot(worldSlot) + 1;
    snprintf(outPath, (size_t)capacity, "%s/player_world_%d.bin", SAVE_DIR, slot);
}

static void EnsureSaveDirectory(void) {
    mkdir(SAVE_DIR, 0755);
}

static bool SaveExistsAtPath(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    fclose(fp);
    return true;
}

static void SetupPlayerCameras(Player *p) {
    p->worldCamera.target = p->pos;
    p->worldCamera.offset = (Vector2){SCREEN_W / 2.0f, SCREEN_H / 2.0f};
    p->worldCamera.rotation = 0.0f;
    p->worldCamera.zoom = 1.0f;

    p->craftCamera.target = (Vector2){0, 0};
    p->craftCamera.offset = (Vector2){440, 230};
    p->craftCamera.zoom = 1.0f;
}

static void ConfigureDefaultSpellLoadout(Player *p) {
    for (int i = 0; i < 10; i++) {
        p->hotbar[i].type = ITEM_SPELL;
        p->hotbar[i].spell.graph.count = 0;

        for (int j = 0; j < MAX_NODES; j++) {
            InitDefaultSpellNode(&p->hotbar[i].spell.graph.nodes[j]);
            p->hotbar[i].spell.graph.nodes[j].active = false;
        }

        SpellNode *root = &p->hotbar[i].spell.graph.nodes[0];
        InitDefaultSpellNode(root);
        root->active = true;
        root->parentId = -1;
        root->pos = (Vector2){0, 0};
        root->temp = 20.0f;
        root->movement = MOVE_STRAIGHT;

        p->hotbar[i].spell.form = FORM_PROJECTILE;
    }
}

static void InitializePlayer(Player *p, int id, Vector2 startPos) {
    memset(p, 0, sizeof(*p));

    p->id = id;
    p->pos = startPos;
    p->z = 0.0f;
    p->speed = 200.0f;
    p->health = 100.0f;
    p->maxHealth = 100.0f;
    p->activeSlot = 0;
    p->castLayer = LAYER_GROUND;
    p->craftCategory = 0;
    p->selectedNodeId = 0;
    p->draggingNodeId = -1;
    p->friendlyFire = false;
    p->showCompendium = false;
    p->aimDir = (Vector2){1.0f, 0.0f};

    for (int i = 0; i < 10; i++) p->editStates[i] = false;

    ConfigureDefaultSpellLoadout(p);
    SetupPlayerCameras(p);
}

static void CopyPlayerLoadout(Player *dst, const Player *src) {
    for (int i = 0; i < 10; i++) dst->hotbar[i] = src->hotbar[i];
}

static bool SavePlayerProfile(const char *path, const Player *p) {
    if (!path || !p) return false;

    EnsureSaveDirectory();
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;

    PlayerProfileBlob *blob = (PlayerProfileBlob *)malloc(sizeof(*blob));
    if (!blob) {
        fclose(fp);
        return false;
    }

    memset(blob, 0, sizeof(*blob));
    blob->magic = PROFILE_MAGIC;
    blob->version = PROFILE_VERSION;
    blob->player = *p;

    bool ok = (fwrite(blob, sizeof(*blob), 1, fp) == 1);
    free(blob);
    fclose(fp);
    return ok;
}

static bool LoadPlayerProfile(const char *path, Player *p) {
    if (!path || !p) return false;

    FILE *fp = fopen(path, "rb");
    if (!fp) return false;

    PlayerProfileBlob *blob = (PlayerProfileBlob *)malloc(sizeof(*blob));
    if (!blob) {
        fclose(fp);
        return false;
    }

    bool ok = (fread(blob, sizeof(*blob), 1, fp) == 1);
    fclose(fp);

    if (!ok) {
        free(blob);
        return false;
    }
    if (blob->magic != PROFILE_MAGIC || blob->version != PROFILE_VERSION) {
        free(blob);
        return false;
    }

    *p = blob->player;
    free(blob);
    return true;
}

static void NormalizeLoadedPlayer(Player *p, int id) {
    p->id = id;
    if (p->speed < 10.0f) p->speed = 200.0f;
    if (p->maxHealth < 1.0f) p->maxHealth = 100.0f;
    if (p->health < 1.0f) p->health = p->maxHealth;

    p->isCrafting = false;
    p->showGuide = false;
    p->showCompendium = false;
    p->draggingNodeId = -1;
    p->selectedNodeId = 0;
    p->isCharging = false;
    p->isLifespanCharging = false;
    p->chargeLevel = 0.0f;
    p->lifespanLevel = 0.0f;
    p->aimDir = NormalizeSafe(p->aimDir);

    SanitizePlayerLoadout(p);

    SetupPlayerCameras(p);
}

static void StartWorldSession(Player players[], const char *worldPath, const char *profilePath, bool multiplayer, bool isHost) {
    InitSimulation();
    InitNPCs();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        Vector2 spawn = {
            WORLD_W * 0.5f + ((float)(i % 4) - 1.5f) * 55.0f,
            WORLD_H * 0.5f + (float)(i / 4) * 65.0f
        };
        InitializePlayer(&players[i], i, spawn);
    }

    bool worldLoaded = LoadWorldState(worldPath, &players[0]);
    if (worldLoaded) {
        NormalizeLoadedPlayer(&players[0], 0);
    }

    if (multiplayer && !isHost) {
        Player *profilePlayer = (Player *)malloc(sizeof(*profilePlayer));
        if (profilePlayer) {
            *profilePlayer = players[0];
            if (LoadPlayerProfile(profilePath, profilePlayer)) {
                CopyPlayerLoadout(&players[0], profilePlayer);
                players[0].activeSlot = profilePlayer->activeSlot;
                players[0].castLayer = profilePlayer->castLayer;
                players[0].friendlyFire = profilePlayer->friendlyFire;
                players[0].health = players[0].maxHealth;
                players[0].aimDir = NormalizeSafe(profilePlayer->aimDir);
                SanitizePlayerLoadout(&players[0]);
            }
            free(profilePlayer);
        }
    }

    for (int i = 1; i < MAX_PLAYERS; i++) {
        CopyPlayerLoadout(&players[i], &players[0]);
        players[i].health = 0.0f;
    }
}

static void RevivePlayer(Player *p, Vector2 fallbackPos) {
    if (!p) return;

    if (p->pos.x < 0.0f || p->pos.x > WORLD_W || p->pos.y < 0.0f || p->pos.y > WORLD_H) {
        p->pos = fallbackPos;
    }

    p->z = 0.0f;
    p->zVelocity = 0.0f;
    p->isJumping = false;
    p->health = p->maxHealth;
    p->isCharging = false;
    p->isLifespanCharging = false;
    p->chargeLevel = 0.0f;
    p->lifespanLevel = 0.0f;
}

static bool IsPeerUsingPlayerIndex(const NetSession *net, int playerIndex) {
    if (!net) return false;
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        if (net->peers[i].active && net->peers[i].playerIndex == playerIndex) return true;
    }
    return false;
}

static void RestartSessionWorld(Player players[], const NetSession *net) {
    InitSimulation();
    InitNPCs();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        bool slotActive = (i == 0);
        if (net && net->enabled && i > 0) slotActive = IsPeerUsingPlayerIndex(net, i);

        Vector2 spawn = {
            WORLD_W * 0.5f + ((float)(i % 4) - 1.5f) * 55.0f,
            WORLD_H * 0.5f + (float)(i / 4) * 65.0f
        };

        players[i].pos = spawn;
        players[i].z = 0.0f;
        players[i].zVelocity = 0.0f;
        players[i].isJumping = false;
        players[i].showGuide = false;
        players[i].showCompendium = false;
        players[i].isCrafting = false;
        players[i].draggingNodeId = -1;
        players[i].selectedNodeId = 0;
        players[i].isCharging = false;
        players[i].isLifespanCharging = false;
        players[i].chargeLevel = 0.0f;
        players[i].lifespanLevel = 0.0f;

        players[i].health = slotActive ? players[i].maxHealth : 0.0f;
        SetupPlayerCameras(&players[i]);
    }
}

static bool SocketAddrEqual(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return (a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port);
}

static int FindPeerByAddr(const NetSession *net, const struct sockaddr_in *addr) {
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        if (!net->peers[i].active) continue;
        if (SocketAddrEqual(&net->peers[i].addr, addr)) return i;
    }
    return -1;
}

static int FindPeerByToken(const NetSession *net, uint32_t token) {
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        if (!net->peers[i].active) continue;
        if (net->peers[i].token == token) return i;
    }
    return -1;
}

static int FindFreePeerSlot(const NetSession *net) {
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        if (!net->peers[i].active) return i;
    }
    return -1;
}

static bool PlayerIndexUsedByPeers(const NetSession *net, int playerIndex) {
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        if (!net->peers[i].active) continue;
        if (net->peers[i].playerIndex == playerIndex) return true;
    }
    return false;
}

static int AcquireHostPlayerIndex(const NetSession *net) {
    for (int i = 1; i < MAX_PLAYERS; i++) {
        if (!PlayerIndexUsedByPeers(net, i)) return i;
    }
    return -1;
}

static int AcquireClientPlayerIndex(const NetSession *net) {
    for (int i = 1; i < MAX_PLAYERS; i++) {
        if (!PlayerIndexUsedByPeers(net, i)) return i;
    }
    return -1;
}

static int EnsureHostPeer(NetSession *net, const struct sockaddr_in *addr, uint32_t token, Player players[]) {
    int existing = FindPeerByAddr(net, addr);
    if (existing != -1) {
        net->peers[existing].token = token;
        net->peers[existing].lastSeenTime = (float)GetTime();
        return existing;
    }

    int freePeer = FindFreePeerSlot(net);
    int freePlayer = AcquireHostPlayerIndex(net);
    if (freePeer == -1 || freePlayer == -1) return -1;

    NetPeer *peer = &net->peers[freePeer];
    memset(peer, 0, sizeof(*peer));
    peer->active = true;
    peer->addr = *addr;
    peer->token = token;
    peer->playerIndex = freePlayer;
    peer->lastSeq = 0;
    peer->lastSeenTime = (float)GetTime();

    InitializePlayer(&players[freePlayer], freePlayer,
                     (Vector2){players[0].pos.x + 60.0f * freePlayer, players[0].pos.y + 30.0f});
    CopyPlayerLoadout(&players[freePlayer], &players[0]);
    players[freePlayer].health = players[freePlayer].maxHealth;

    return freePeer;
}

static int EnsureClientPeer(NetSession *net, uint32_t token, Player players[]) {
    if (token == net->localToken) return -1;

    int existing = FindPeerByToken(net, token);
    if (existing != -1) {
        net->peers[existing].lastSeenTime = (float)GetTime();
        return existing;
    }

    int freePeer = FindFreePeerSlot(net);
    int freePlayer = AcquireClientPlayerIndex(net);
    if (freePeer == -1 || freePlayer == -1) return -1;

    NetPeer *peer = &net->peers[freePeer];
    memset(peer, 0, sizeof(*peer));
    peer->active = true;
    peer->token = token;
    peer->playerIndex = freePlayer;
    peer->lastSeq = 0;
    peer->lastSeenTime = (float)GetTime();

    InitializePlayer(&players[freePlayer], freePlayer,
                     (Vector2){players[0].pos.x + 60.0f * freePlayer, players[0].pos.y + 30.0f});
    CopyPlayerLoadout(&players[freePlayer], &players[0]);
    players[freePlayer].health = 0.0f;

    return freePeer;
}

static int CountConnectedPeers(const NetSession *net) {
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS - 1; i++) if (net->peers[i].active) count++;
    return count;
}

static void PruneInactivePeers(NetSession *net, Player players[]) {
    float now = (float)GetTime();
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        if (!net->peers[i].active) continue;
        if (now - net->peers[i].lastSeenTime <= 18.0f) continue;

        int idx = net->peers[i].playerIndex;
        if (idx >= 1 && idx < MAX_PLAYERS) players[idx].health = 0.0f;
        net->peers[i].active = false;
    }
}

static bool NetInit(NetSession *net, NetMode mode, const char *joinIp) {
    memset(net, 0, sizeof(*net));
    net->enabled = (mode != NET_DISABLED);
    net->mode = mode;
    net->socketFd = -1;

    if (!net->enabled) return true;

    net->localToken = (uint32_t)GetRandomValue(1000000, 2000000000);
    net->localToken ^= (uint32_t)(GetTime() * 1000.0);

    net->socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (net->socketFd < 0) {
        net->enabled = false;
        return false;
    }

    int flags = fcntl(net->socketFd, F_GETFL, 0);
    fcntl(net->socketFd, F_SETFL, flags | O_NONBLOCK);

    if (mode == NET_HOST) {
        int reuse = 1;
        setsockopt(net->socketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(NET_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(net->socketFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(net->socketFd);
            net->socketFd = -1;
            net->enabled = false;
            return false;
        }
    } else {
        struct sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(0);
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(net->socketFd, (struct sockaddr *)&localAddr, sizeof(localAddr));

        memset(&net->hostAddr, 0, sizeof(net->hostAddr));
        net->hostAddr.sin_family = AF_INET;
        net->hostAddr.sin_port = htons(NET_PORT);
        net->hostAddr.sin_addr.s_addr = inet_addr(joinIp);
        net->hasHost = true;
    }

    net->seq = 0;
    return true;
}

static void NetShutdown(NetSession *net) {
    if (net->socketFd >= 0) {
        close(net->socketFd);
        net->socketFd = -1;
    }
    net->enabled = false;
    net->hasHost = false;
    net->mode = NET_DISABLED;
    for (int i = 0; i < MAX_PLAYERS - 1; i++) net->peers[i].active = false;
}

static void BuildStatePacket(const Player *local, const LocalNetEvents *ev, NetMode mode, uint32_t senderToken, uint32_t seq, NetStatePacket *pkt) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->h.magic = NET_MAGIC;
    pkt->h.packetType = PACKET_STATE;
    pkt->h.seq = seq;
    pkt->h.senderToken = senderToken;
    pkt->h.senderMode = mode;

    pkt->x = local->pos.x;
    pkt->y = local->pos.y;
    pkt->z = local->z;
    pkt->health = local->health;
    pkt->aimX = local->aimDir.x;
    pkt->aimY = local->aimDir.y;
    pkt->animTime = local->animTime;

    pkt->activeSlot = local->activeSlot;
    pkt->castLayer = local->castLayer;
    pkt->chargeLevel = local->chargeLevel;
    pkt->lifespanLevel = local->lifespanLevel;

    pkt->castEvent = ev->cast ? 1 : 0;
    pkt->castTargetX = ev->castTarget.x;
    pkt->castTargetY = ev->castTarget.y;
    pkt->castChargeMult = ev->castChargeMult;
    pkt->castLifeMult = ev->castLifeMult;

    pkt->spawnNpcEvent = ev->spawnNpc ? 1 : 0;
    pkt->npcSpawnX = ev->npcSpawnPos.x;
    pkt->npcSpawnY = ev->npcSpawnPos.y;

    pkt->reviveEvent = ev->revive ? 1 : 0;
    pkt->restartEvent = ev->restart ? 1 : 0;
}

static void BuildLoadoutPacket(const Player *local, const LocalNetEvents *ev, NetMode mode, uint32_t senderToken, uint32_t seq, NetLoadoutPacket *pkt) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->h.magic = NET_MAGIC;
    pkt->h.packetType = PACKET_LOADOUT;
    pkt->h.seq = seq;
    pkt->h.senderToken = senderToken;
    pkt->h.senderMode = mode;

    pkt->slotIndex = ClampSlotIndex(ev->loadoutSlot);
    pkt->activeSlot = ClampSlotIndex(local->activeSlot);
    pkt->slotData = ev->loadoutData;
}

static void ApplyRemoteState(Player *remote, const NetStatePacket *pkt) {
    remote->pos = (Vector2){pkt->x, pkt->y};
    remote->z = pkt->z;
    remote->health = pkt->health;
    remote->activeSlot = ClampSlotIndex(pkt->activeSlot);
    remote->castLayer = pkt->castLayer;
    remote->chargeLevel = pkt->chargeLevel;
    remote->lifespanLevel = pkt->lifespanLevel;
    remote->aimDir = NormalizeSafe((Vector2){pkt->aimX, pkt->aimY});
    remote->animTime = pkt->animTime;
}

static void ApplyRemoteLoadout(Player *remote, const NetLoadoutPacket *pkt) {
    int slot = ClampSlotIndex(pkt->slotIndex);
    remote->hotbar[slot] = pkt->slotData;
    SanitizeHotbarSlot(&remote->hotbar[slot]);
    remote->activeSlot = ClampSlotIndex(pkt->activeSlot);
}

static void BroadcastToPeersRaw(NetSession *net, const void *data, size_t size) {
    if (!net->enabled || net->mode != NET_HOST) return;
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        if (!net->peers[i].active) continue;
        sendto(net->socketFd, data, size, 0, (struct sockaddr *)&net->peers[i].addr, sizeof(net->peers[i].addr));
    }
}

static void SendToHostRaw(NetSession *net, const void *data, size_t size) {
    if (!net->enabled || net->mode != NET_CLIENT || !net->hasHost) return;
    sendto(net->socketFd, data, size, 0, (struct sockaddr *)&net->hostAddr, sizeof(net->hostAddr));
}

static void NetUpdate(NetSession *net, Player players[], LocalNetEvents *localEvents, bool *incomingRestart) {
    if (!net->enabled || net->socketFd < 0) return;

    NetStatePacket sendState;
    BuildStatePacket(&players[0], localEvents, net->mode, net->localToken, ++net->seq, &sendState);
    if (net->mode == NET_HOST) BroadcastToPeersRaw(net, &sendState, sizeof(sendState));
    else SendToHostRaw(net, &sendState, sizeof(sendState));

    if (localEvents->loadoutSync) {
        NetLoadoutPacket sendLoadout;
        BuildLoadoutPacket(&players[0], localEvents, net->mode, net->localToken, ++net->seq, &sendLoadout);
        if (net->mode == NET_HOST) BroadcastToPeersRaw(net, &sendLoadout, sizeof(sendLoadout));
        else SendToHostRaw(net, &sendLoadout, sizeof(sendLoadout));
    }

    unsigned char recvBuffer[sizeof(NetLoadoutPacket)];
    while (1) {
        struct sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        ssize_t readBytes = recvfrom(net->socketFd, recvBuffer, sizeof(recvBuffer), 0, (struct sockaddr *)&fromAddr, &fromLen);
        if (readBytes <= 0) break;
        if (readBytes < (ssize_t)sizeof(NetPacketHeader)) continue;

        NetPacketHeader *hdr = (NetPacketHeader *)recvBuffer;
        if (hdr->magic != NET_MAGIC) continue;

        bool isStatePacket = (hdr->packetType == PACKET_STATE);
        bool isLoadoutPacket = (hdr->packetType == PACKET_LOADOUT);
        if (!isStatePacket && !isLoadoutPacket) continue;
        if (isStatePacket && readBytes < (ssize_t)sizeof(NetStatePacket)) continue;
        if (isLoadoutPacket && readBytes < (ssize_t)sizeof(NetLoadoutPacket)) continue;

        if (net->mode == NET_HOST) {
            if (hdr->senderToken == net->localToken) continue;
            if (hdr->senderMode != NET_CLIENT) continue;

            int peerId = EnsureHostPeer(net, &fromAddr, hdr->senderToken, players);
            if (peerId < 0) continue;

            NetPeer *peer = &net->peers[peerId];
            if (hdr->seq <= peer->lastSeq) continue;
            peer->lastSeq = hdr->seq;
            peer->lastSeenTime = (float)GetTime();

            int playerIdx = peer->playerIndex;

            if (isStatePacket) {
                NetStatePacket *pkt = (NetStatePacket *)recvBuffer;

                if (pkt->reviveEvent) {
                    RevivePlayer(&players[playerIdx], (Vector2){WORLD_W * 0.5f, WORLD_H * 0.5f});
                    pkt->health = players[playerIdx].health;
                    pkt->z = players[playerIdx].z;
                }

                ApplyRemoteState(&players[playerIdx], pkt);

                if (pkt->castEvent && players[playerIdx].hotbar[players[playerIdx].activeSlot].type == ITEM_SPELL) {
                    ExecuteSpell(&players[playerIdx], (Vector2){pkt->castTargetX, pkt->castTargetY},
                                 &players[playerIdx].hotbar[players[playerIdx].activeSlot].spell,
                                 pkt->castChargeMult, pkt->castLifeMult);
                }

                if (pkt->spawnNpcEvent && players[playerIdx].hotbar[players[playerIdx].activeSlot].type == ITEM_NPC) {
                    SpawnNPC((Vector2){pkt->npcSpawnX, pkt->npcSpawnY}, players[playerIdx].hotbar[players[playerIdx].activeSlot].npc);
                }
            } else {
                NetLoadoutPacket *pkt = (NetLoadoutPacket *)recvBuffer;
                ApplyRemoteLoadout(&players[playerIdx], pkt);
            }

            BroadcastToPeersRaw(net, recvBuffer, (size_t)readBytes);
        } else {
            if (net->hasHost && !SocketAddrEqual(&fromAddr, &net->hostAddr)) continue;

            if (hdr->senderToken == net->localToken) {
                if (isStatePacket) {
                    NetStatePacket *pkt = (NetStatePacket *)recvBuffer;
                    if (pkt->castEvent && players[0].hotbar[players[0].activeSlot].type == ITEM_SPELL) {
                        ExecuteSpell(&players[0], (Vector2){pkt->castTargetX, pkt->castTargetY},
                                     &players[0].hotbar[players[0].activeSlot].spell,
                                     pkt->castChargeMult, pkt->castLifeMult);
                    }
                    if (pkt->spawnNpcEvent && players[0].hotbar[players[0].activeSlot].type == ITEM_NPC) {
                        SpawnNPC((Vector2){pkt->npcSpawnX, pkt->npcSpawnY}, players[0].hotbar[players[0].activeSlot].npc);
                    }
                    if (pkt->reviveEvent) RevivePlayer(&players[0], players[0].pos);
                    if (pkt->restartEvent && incomingRestart) *incomingRestart = true;
                } else {
                    NetLoadoutPacket *pkt = (NetLoadoutPacket *)recvBuffer;
                    ApplyRemoteLoadout(&players[0], pkt);
                }
                continue;
            }

            int peerId = EnsureClientPeer(net, hdr->senderToken, players);
            if (peerId < 0) continue;

            NetPeer *peer = &net->peers[peerId];
            if (hdr->seq <= peer->lastSeq) continue;
            peer->lastSeq = hdr->seq;
            peer->lastSeenTime = (float)GetTime();

            int playerIdx = peer->playerIndex;

            if (isStatePacket) {
                NetStatePacket *pkt = (NetStatePacket *)recvBuffer;
                ApplyRemoteState(&players[playerIdx], pkt);

                if (pkt->castEvent && players[playerIdx].hotbar[players[playerIdx].activeSlot].type == ITEM_SPELL) {
                    ExecuteSpell(&players[playerIdx], (Vector2){pkt->castTargetX, pkt->castTargetY},
                                 &players[playerIdx].hotbar[players[playerIdx].activeSlot].spell,
                                 pkt->castChargeMult, pkt->castLifeMult);
                }

                if (pkt->spawnNpcEvent && players[playerIdx].hotbar[players[playerIdx].activeSlot].type == ITEM_NPC) {
                    SpawnNPC((Vector2){pkt->npcSpawnX, pkt->npcSpawnY}, players[playerIdx].hotbar[players[playerIdx].activeSlot].npc);
                }

                if (pkt->reviveEvent) {
                    RevivePlayer(&players[playerIdx], players[playerIdx].pos);
                }

                if (pkt->restartEvent && hdr->senderMode == NET_HOST && incomingRestart) {
                    *incomingRestart = true;
                }
            } else {
                NetLoadoutPacket *pkt = (NetLoadoutPacket *)recvBuffer;
                ApplyRemoteLoadout(&players[playerIdx], pkt);
            }
        }
    }

    PruneInactivePeers(net, players);
}

static bool DrawMenuButton(Rectangle rec, const char *label, Vector2 mouseVirtual) {
    bool hover = CheckCollisionPointRec(mouseVirtual, rec);
    Color fill = hover ? (Color){55, 60, 70, 240} : (Color){30, 35, 45, 240};
    Color border = hover ? GOLD : DARKGRAY;

    DrawRectangleRec(rec, fill);
    DrawRectangleLinesEx(rec, 2.0f, border);

    int tw = MeasureText(label, 20);
    DrawText(label, (int)(rec.x + rec.width * 0.5f - tw * 0.5f), (int)(rec.y + rec.height * 0.5f - 10), 20, RAYWHITE);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static bool DrawChoiceButton(Rectangle rec, const char *label, Vector2 mouseVirtual, bool selected) {
    bool hover = CheckCollisionPointRec(mouseVirtual, rec);
    Color fill = selected ? (Color){40, 65, 55, 235} : (Color){22, 28, 36, 235};
    if (hover) fill = selected ? (Color){52, 82, 68, 245} : (Color){36, 42, 54, 245};

    DrawRectangleRec(rec, fill);
    DrawRectangleLinesEx(rec, 2.0f, selected ? GOLD : DARKGRAY);

    int tw = MeasureText(label, 16);
    DrawText(label, (int)(rec.x + rec.width * 0.5f - tw * 0.5f), (int)(rec.y + rec.height * 0.5f - 8), 16, RAYWHITE);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static int DrawWorldSelector(int selectedSlot, float x, float y, Vector2 mouseVirtual) {
    DrawText("WORLD SLOT", (int)x, (int)y, 12, SKYBLUE);

    int current = ClampWorldSlot(selectedSlot);
    for (int i = 0; i < WORLD_SLOT_COUNT; i++) {
        Rectangle rec = {x + i * 110.0f, y + 18.0f, 100.0f, 30.0f};
        if (DrawChoiceButton(rec, TextFormat("WORLD %d", i + 1), mouseVirtual, current == i)) current = i;
    }
    return current;
}

static void DrawMenuBackdrop(float t) {
    for (int y = 0; y < SCREEN_H; y++) {
        float k = (float)y / (float)SCREEN_H;
        Color c = {
            (unsigned char)(12 + 18 * (1.0f - k)),
            (unsigned char)(14 + 24 * (1.0f - k)),
            (unsigned char)(26 + 28 * k),
            255
        };
        DrawLine(0, y, SCREEN_W, y, c);
    }

    for (int i = 0; i < 24; i++) {
        float px = 40.0f + i * 32.0f + sinf(t + i * 0.7f) * 18.0f;
        float py = 60.0f + fmodf(t * (12.0f + i), (float)SCREEN_H);
        DrawCircle(px, py, 2.0f + (i % 3), Fade(SKYBLUE, 0.35f));
    }
}

static bool IsJoinCharValid(int c) {
    if (c >= '0' && c <= '9') return true;
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    return (c == '.' || c == ':' || c == '-');
}

static void UpdateJoinAddressInput(char *joinAddress, int capacity, bool *editing, Rectangle inputRec, Vector2 virtualMouse) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *editing = CheckCollisionPointRec(virtualMouse, inputRec);
    }

    if (!(*editing)) return;

    int key = GetCharPressed();
    while (key > 0) {
        if (IsJoinCharValid(key)) {
            int len = (int)strlen(joinAddress);
            if (len < capacity - 1) {
                joinAddress[len] = (char)key;
                joinAddress[len + 1] = '\0';
            }
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen(joinAddress);
        if (len > 0) joinAddress[len - 1] = '\0';
    }
}

int main() {
#if defined(PLATFORM_ANDROID)
    app_dummy();
#endif
#if !defined(PLATFORM_ANDROID)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
    InitWindow(SCREEN_W, SCREEN_H, "Metsys: Open World Ecosystem");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    RenderTexture2D target = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    static Player players[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Vector2 spawn = {
            WORLD_W * 0.5f + ((float)(i % 4) - 1.5f) * 55.0f,
            WORLD_H * 0.5f + (float)(i / 4) * 65.0f
        };
        InitializePlayer(&players[i], i, spawn);
        if (i > 0) players[i].health = 0.0f;
    }

    NetSession net;
    memset(&net, 0, sizeof(net));
    net.socketFd = -1;

    AppScreen appScreen = APP_MAIN_MENU;
    NPCDNA draftNPC = {50, 0, 0, 50, 50, 0};

    int selectedWorldSlot = 0;
    bool multiplayerOnlineMode = false;

    char activeWorldPath[128];
    char activeProfilePath[128];
    BuildWorldSavePath(selectedWorldSlot, activeWorldPath, (int)sizeof(activeWorldPath));
    BuildProfileSavePath(selectedWorldSlot, activeProfilePath, (int)sizeof(activeProfilePath));

    bool wantsRestart = false;
    bool wantsReturnToMenu = false;
    bool wantsRevive = false;
    bool pauseShowAbout = false;
    bool pauseShowMore = false;

    bool pendingNetRestart = false;
    bool pendingNetRevive = false;

    float autosaveTimer = 0.0f;

    char joinAddress[64] = "127.0.0.1";
    bool editingJoinAddress = false;

    uint32_t localHotbarHashes[10] = {0};
    bool forceHotbarSyncAll = true;
    int forceHotbarSyncCursor = 0;
    int lastHostPeerCount = 0;

    MobileControlsState mobileControls;
    memset(&mobileControls, 0, sizeof(mobileControls));
    SeedHotbarHashes(&players[0], localHotbarHashes);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        float scale = fminf((float)GetScreenWidth() / SCREEN_W, (float)GetScreenHeight() / SCREEN_H);
        float offsetX = (GetScreenWidth() - ((float)SCREEN_W * scale)) * 0.5f;
        float offsetY = (GetScreenHeight() - ((float)SCREEN_H * scale)) * 0.5f;

        SetMouseOffset((int)-offsetX, (int)-offsetY);
        SetMouseScale(1.0f / scale, 1.0f / scale);

        Vector2 virtualMouse = GetMousePosition();
        virtualMouse.x = fmaxf(0.0f, fminf(virtualMouse.x, SCREEN_W));
        virtualMouse.y = fmaxf(0.0f, fminf(virtualMouse.y, SCREEN_H));

        UpdateMobileControls(&mobileControls, scale, offsetX, offsetY);

#if !defined(PLATFORM_ANDROID)
        if (IsKeyPressed(KEY_F11)) {
            if (!IsWindowFullscreen()) {
                int display = GetCurrentMonitor();
                SetWindowSize(GetMonitorWidth(display), GetMonitorHeight(display));
                ToggleFullscreen();
            } else {
                ToggleFullscreen();
                SetWindowSize(SCREEN_W, SCREEN_H);
            }
        }
#endif

        Player *p1 = &players[0];
        bool localIsHost = (!net.enabled || net.mode == NET_HOST);
        LocalNetEvents localEvents = {0};

        if (appScreen == APP_GAME) {
            SanitizePlayerLoadout(p1);

            if (IsKeyPressed(KEY_F9) || mobileControls.menuPressed) wantsReturnToMenu = true;

            if (wantsReturnToMenu) {
                if (localIsHost) SaveWorldState(activeWorldPath, p1);
                SavePlayerProfile(activeProfilePath, p1);

                NetShutdown(&net);
                appScreen = APP_MAIN_MENU;

                wantsReturnToMenu = false;
                wantsRestart = false;
                wantsRevive = false;
                pauseShowAbout = false;
                pauseShowMore = false;
                autosaveTimer = 0.0f;
                continue;
            }

            if (IsKeyPressed(KEY_F5)) {
                if (localIsHost) SaveWorldState(activeWorldPath, p1);
                SavePlayerProfile(activeProfilePath, p1);
            }

            if (wantsRestart) {
                if (localIsHost) {
                    RestartSessionWorld(players, &net);
                    if (net.enabled && net.mode == NET_HOST) pendingNetRestart = true;
                }
                wantsRestart = false;
            }

            if (wantsRevive) {
                RevivePlayer(p1, (Vector2){WORLD_W * 0.5f, WORLD_H * 0.5f});
                if (net.enabled) pendingNetRevive = true;
                wantsRevive = false;
            }

            if (IsKeyPressed(KEY_ESCAPE) || mobileControls.pausePressed) {
                p1->showGuide = !p1->showGuide;
                if (!p1->showGuide) {
                    pauseShowAbout = false;
                    pauseShowMore = false;
                }
            }

            if ((IsKeyPressed(KEY_GRAVE) || mobileControls.craftPressed) && !p1->showGuide && p1->health > 0) {
                p1->isCrafting = !p1->isCrafting;
                p1->draggingNodeId = -1;
            }

            if (IsKeyPressed(KEY_TAB)) {
                if (p1->isCrafting) {
                    p1->craftCategory = (p1->craftCategory == 0) ? 1 : 0;
                } else {
                    if (p1->visionBlend < 0.5f) p1->visionBlend = 1.0f;
                    else if (p1->visionBlend < 1.5f) p1->visionBlend = 2.0f;
                    else p1->visionBlend = 0.0f;
                }
            }

            if (IsKeyPressed(KEY_LEFT_SHIFT) || mobileControls.layerPressed) p1->castLayer = !p1->castLayer;
            if (IsKeyDown(KEY_RIGHT_BRACKET)) p1->visionBlend = fminf(2.0f, p1->visionBlend + dt * 1.5f);
            if (IsKeyDown(KEY_LEFT_BRACKET)) p1->visionBlend = fmaxf(0.0f, p1->visionBlend - dt * 1.5f);

            if (!p1->isCrafting && !p1->showGuide && p1->health > 0) {
                if ((IsKeyPressed(KEY_SPACE) || mobileControls.jumpPressed) && !p1->isJumping) {
                    p1->isJumping = true;
                    p1->zVelocity = 250.0f;
                }

                Vector2 delta = {0};
                if (IsKeyDown(KEY_W) || mobileControls.up) delta.y -= p1->speed * dt;
                if (IsKeyDown(KEY_S) || mobileControls.down) delta.y += p1->speed * dt;
                if (IsKeyDown(KEY_A) || mobileControls.left) delta.x -= p1->speed * dt;
                if (IsKeyDown(KEY_D) || mobileControls.right) delta.x += p1->speed * dt;
                MovePlayer(p1, delta, dt);

                for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) p1->activeSlot = i;
                if (IsKeyPressed(KEY_ZERO)) p1->activeSlot = 9;

                HotbarSlot *active = &p1->hotbar[p1->activeSlot];
                Vector2 worldMouse = GetScreenToWorld2D(virtualMouse, p1->worldCamera);
                if (mobileControls.hasAim) worldMouse = GetScreenToWorld2D(mobileControls.aimVirtual, p1->worldCamera);
#if defined(PLATFORM_ANDROID)
                if (!mobileControls.hasAim) {
                    worldMouse = (Vector2){
                        p1->pos.x + p1->aimDir.x * 120.0f,
                        p1->pos.y + p1->aimDir.y * 120.0f
                    };
                }
#endif
                p1->aimDir = NormalizeSafe((Vector2){worldMouse.x - p1->pos.x, worldMouse.y - p1->pos.y});

                if (active->type == ITEM_SPELL) {
                    bool holdingLMB = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || mobileControls.castHold;
                    bool holdingC = IsKeyDown(KEY_C) || mobileControls.lifeHold;

                    if (holdingLMB) {
                        p1->isCharging = true;
                        p1->chargeLevel = fminf(4.0f, p1->chargeLevel + dt * 2.0f);
                    } else {
                        p1->isCharging = false;
                        p1->chargeLevel = fmaxf(0.0f, p1->chargeLevel - dt * 4.0f);
                    }

                    if (holdingC) {
                        p1->isLifespanCharging = true;
                        p1->lifespanLevel = fminf(4.0f, p1->lifespanLevel + dt * 2.0f);
                    } else {
                        p1->isLifespanCharging = false;
                        p1->lifespanLevel = fmaxf(0.0f, p1->lifespanLevel - dt * 4.0f);
                    }

                    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || mobileControls.castReleased) {
                        float chargeMult = 1.0f + p1->chargeLevel;
                        float lifeMult = 1.0f + p1->lifespanLevel;

                        if (!net.enabled || net.mode == NET_HOST) {
                            ExecuteSpell(p1, worldMouse, &active->spell, chargeMult, lifeMult);
                        }

                        localEvents.cast = true;
                        localEvents.castTarget = worldMouse;
                        localEvents.castChargeMult = chargeMult;
                        localEvents.castLifeMult = lifeMult;

                        p1->chargeLevel = 0.0f;
                        p1->lifespanLevel = 0.0f;
                    }
                } else if (active->type == ITEM_NPC) {
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || mobileControls.castPressed) {
                        if (!net.enabled || net.mode == NET_HOST) {
                            SpawnNPC(worldMouse, active->npc);
                        }
                        localEvents.spawnNpc = true;
                        localEvents.npcSpawnPos = worldMouse;
                    }
                }
            }

            localEvents.revive = pendingNetRevive;
            localEvents.restart = pendingNetRestart;
            pendingNetRevive = false;
            pendingNetRestart = false;

            if (net.enabled) {
                QueueLoadoutSyncEvent(p1, &localEvents, localHotbarHashes, &forceHotbarSyncAll, &forceHotbarSyncCursor);
            } else {
                localEvents.loadoutSync = false;
                forceHotbarSyncAll = true;
                forceHotbarSyncCursor = 0;
                lastHostPeerCount = 0;
                SeedHotbarHashes(p1, localHotbarHashes);
            }

            bool incomingRestart = false;
            if (net.enabled) {
                NetUpdate(&net, players, &localEvents, &incomingRestart);

                if (net.mode == NET_HOST) {
                    int peerCount = CountConnectedPeers(&net);
                    if (peerCount > lastHostPeerCount) {
                        forceHotbarSyncAll = true;
                        forceHotbarSyncCursor = 0;
                    }
                    lastHostPeerCount = peerCount;
                } else {
                    lastHostPeerCount = 0;
                }
            }
            if (incomingRestart) RestartSessionWorld(players, &net);

            p1->worldCamera.target = p1->pos;

            int simPlayerCount = net.enabled ? MAX_PLAYERS : 1;
            UpdateSimulation(dt, players, simPlayerCount);
            UpdateNPCs(dt, players, simPlayerCount);

            autosaveTimer += dt;
            if (autosaveTimer > 8.0f) {
                if (localIsHost) SaveWorldState(activeWorldPath, p1);
                SavePlayerProfile(activeProfilePath, p1);
                autosaveTimer = 0.0f;
            }
        }

        BeginTextureMode(target);
            ClearBackground((Color){20, 20, 25, 255});

            if (appScreen == APP_MAIN_MENU) {
                char selectedWorldPath[128];
                BuildWorldSavePath(selectedWorldSlot, selectedWorldPath, (int)sizeof(selectedWorldPath));

                DrawMenuBackdrop(GetTime());
                DrawText("METSYS ENGINE", 258, 66, 42, GOLD);
                DrawText("Temporal Graph Spellcraft Sandbox", 260, 114, 16, SKYBLUE);

                selectedWorldSlot = DrawWorldSelector(selectedWorldSlot, 235.0f, 148.0f, virtualMouse);

                Rectangle myWorldRec = {280, 205, 240, 45};
                Rectangle multiRec = {280, 260, 240, 45};
                Rectangle othersRec = {280, 315, 240, 45};
                Rectangle exitRec = {280, 370, 240, 45};

                if (DrawMenuButton(myWorldRec, "MY WORLD", virtualMouse)) {
                    BuildWorldSavePath(selectedWorldSlot, activeWorldPath, (int)sizeof(activeWorldPath));
                    BuildProfileSavePath(selectedWorldSlot, activeProfilePath, (int)sizeof(activeProfilePath));

                    NetShutdown(&net);
                    StartWorldSession(players, activeWorldPath, activeProfilePath, false, true);
                    SeedHotbarHashes(&players[0], localHotbarHashes);
                    forceHotbarSyncAll = true;
                    forceHotbarSyncCursor = 0;
                    lastHostPeerCount = 0;

                    wantsRestart = false;
                    wantsReturnToMenu = false;
                    wantsRevive = false;
                    pauseShowAbout = false;
                    pauseShowMore = false;
                    autosaveTimer = 0.0f;
                    appScreen = APP_GAME;
                }

                if (DrawMenuButton(multiRec, "MULTIPLAYER", virtualMouse)) appScreen = APP_MULTI_MENU;
                if (DrawMenuButton(othersRec, "OTHERS", virtualMouse)) appScreen = APP_OTHERS_MENU;
                if (DrawMenuButton(exitRec, "EXIT", virtualMouse)) {
                    EndTextureMode();
                    break;
                }

                DrawText(SaveExistsAtPath(selectedWorldPath) ? "WORLD SAVE: FOUND" : "WORLD SAVE: NONE", 20, 420, 10, LIGHTGRAY);
            }

            else if (appScreen == APP_MULTI_MENU) {
                DrawMenuBackdrop(GetTime() * 0.8f);
                DrawText("MULTIPLAYER", 300, 62, 36, GOLD);
                DrawText("Pick world first, then host or join", 264, 100, 14, SKYBLUE);

                selectedWorldSlot = DrawWorldSelector(selectedWorldSlot, 200.0f, 122.0f, virtualMouse);

                Rectangle lanRec = {190, 180, 190, 34};
                Rectangle onlineRec = {420, 180, 190, 34};
                if (DrawChoiceButton(lanRec, "OFFLINE / LAN", virtualMouse, !multiplayerOnlineMode)) multiplayerOnlineMode = false;
                if (DrawChoiceButton(onlineRec, "ONLINE / DIRECT IP", virtualMouse, multiplayerOnlineMode)) multiplayerOnlineMode = true;

                Rectangle hostRec = {280, 230, 240, 42};
                Rectangle joinRec = {280, 280, 240, 42};
                Rectangle backRec = {280, 370, 240, 38};
                Rectangle inputRec = {230, 332, 340, 30};

                UpdateJoinAddressInput(joinAddress, (int)sizeof(joinAddress), &editingJoinAddress, inputRec, virtualMouse);

                DrawRectangleRec(inputRec, Fade(BLACK, 0.8f));
                DrawRectangleLinesEx(inputRec, 2.0f, editingJoinAddress ? GOLD : GRAY);
                DrawText("JOIN ADDRESS", 230, 315, 10, LIGHTGRAY);
                DrawText(joinAddress, 238, 339, 18, RAYWHITE);

                if (DrawMenuButton(hostRec, "HOST SESSION", virtualMouse)) {
                    BuildWorldSavePath(selectedWorldSlot, activeWorldPath, (int)sizeof(activeWorldPath));
                    BuildProfileSavePath(selectedWorldSlot, activeProfilePath, (int)sizeof(activeProfilePath));

                    NetShutdown(&net);
                    if (NetInit(&net, NET_HOST, joinAddress)) {
                        StartWorldSession(players, activeWorldPath, activeProfilePath, true, true);
                        SeedHotbarHashes(&players[0], localHotbarHashes);
                        forceHotbarSyncAll = true;
                        forceHotbarSyncCursor = 0;
                        lastHostPeerCount = 0;
                        wantsRestart = false;
                        wantsReturnToMenu = false;
                        wantsRevive = false;
                        pauseShowAbout = false;
                        pauseShowMore = false;
                        autosaveTimer = 0.0f;
                        appScreen = APP_GAME;
                    }
                }

                if (DrawMenuButton(joinRec, "JOIN SESSION", virtualMouse)) {
                    BuildWorldSavePath(selectedWorldSlot, activeWorldPath, (int)sizeof(activeWorldPath));
                    BuildProfileSavePath(selectedWorldSlot, activeProfilePath, (int)sizeof(activeProfilePath));

                    NetShutdown(&net);
                    if (NetInit(&net, NET_CLIENT, joinAddress)) {
                        StartWorldSession(players, activeWorldPath, activeProfilePath, true, false);
                        SeedHotbarHashes(&players[0], localHotbarHashes);
                        forceHotbarSyncAll = true;
                        forceHotbarSyncCursor = 0;
                        lastHostPeerCount = 0;
                        wantsRestart = false;
                        wantsReturnToMenu = false;
                        wantsRevive = false;
                        pauseShowAbout = false;
                        pauseShowMore = false;
                        autosaveTimer = 0.0f;
                        appScreen = APP_GAME;
                    }
                }

                if (DrawMenuButton(backRec, "BACK", virtualMouse)) appScreen = APP_MAIN_MENU;

                DrawText(multiplayerOnlineMode ? "Mode: Online direct-IP" : "Mode: Offline LAN", 290, 414, 10, LIGHTGRAY);
            }

            else if (appScreen == APP_OTHERS_MENU) {
                char selectedWorldPath[128];
                char selectedProfilePath[128];
                BuildWorldSavePath(selectedWorldSlot, selectedWorldPath, (int)sizeof(selectedWorldPath));
                BuildProfileSavePath(selectedWorldSlot, selectedProfilePath, (int)sizeof(selectedProfilePath));

                DrawMenuBackdrop(GetTime() * 0.6f);
                DrawText("OTHERS", 335, 65, 36, GOLD);

                selectedWorldSlot = DrawWorldSelector(selectedWorldSlot, 235.0f, 108.0f, virtualMouse);

                DrawRectangle(90, 165, 620, 165, Fade(BLACK, 0.8f));
                DrawRectangleLines(90, 165, 620, 165, SKYBLUE);
                DrawText("COMPENDIUM SNAPSHOT", 110, 178, 16, SKYBLUE);
                DrawText("SPD speed | DLY delay | SPR spread | DST distortion | RNG range | SIZ size", 110, 206, 10, RAYWHITE);
                DrawText("COND ALW always | T> time gate | S>/S< scalar thresholds", 110, 222, 10, RAYWHITE);
                DrawText("CH TMP/MAS/WET/COH/CHG chooses scalar channel", 110, 238, 10, RAYWHITE);
                DrawText("TOOLS BLD DIG WET DRY HOT COO CON BHO WHO", 110, 254, 10, RAYWHITE);
                DrawText("PAUSE MENU NOW HAS: RETURN, ABOUT, MORE, REVIVE", 110, 270, 10, GOLD);

                Rectangle eraseWorldRec = {140, 340, 240, 40};
                Rectangle eraseProfileRec = {420, 340, 240, 40};
                Rectangle backRec = {300, 390, 200, 34};

                if (DrawMenuButton(eraseWorldRec, "ERASE WORLD SAVE", virtualMouse)) remove(selectedWorldPath);
                if (DrawMenuButton(eraseProfileRec, "ERASE PLAYER SAVE", virtualMouse)) remove(selectedProfilePath);
                if (DrawMenuButton(backRec, "BACK", virtualMouse)) appScreen = APP_MAIN_MENU;
            }

            else if (appScreen == APP_GAME) {
                float matAlpha = 1.0f - (fminf(p1->visionBlend, 2.0f) * 0.4f);
                float nrgAlpha = 1.0f - fabsf(p1->visionBlend - 1.0f);
                float hazAlpha = fmaxf(0.0f, p1->visionBlend - 1.0f);

                BeginMode2D(p1->worldCamera);
                    DrawMaterialRealm(matAlpha, p1->worldCamera);
                    if (nrgAlpha > 0.01f) DrawEnergyRealm(nrgAlpha, p1->worldCamera);
                    if (hazAlpha > 0.01f) DrawHazardRealm(hazAlpha, p1->worldCamera);

                    DrawSingularities(1.0f);
                    DrawNPCs();
                    DrawProjectiles(p1);

                    DrawPlayerEntity(p1);

                    if (net.enabled) {
                        for (int i = 1; i < MAX_PLAYERS; i++) {
                            if (players[i].health <= 0.0f) continue;
                            DrawPlayerEntity(&players[i]);
                            DrawCircleLines(players[i].pos.x, players[i].pos.y - players[i].z, 10.0f, YELLOW);
                            DrawLineEx((Vector2){players[i].pos.x, players[i].pos.y - players[i].z},
                                       (Vector2){players[i].pos.x + players[i].aimDir.x * 20.0f,
                                                 players[i].pos.y - players[i].z + players[i].aimDir.y * 20.0f},
                                       1.5f, YELLOW);
                        }
                    }
                EndMode2D();

                if (!p1->isCrafting && !p1->showGuide && p1->health > 0 && p1->hotbar[p1->activeSlot].type == ITEM_NPC) {
                    DrawProceduralNPC(virtualMouse, 0, p1->hotbar[p1->activeSlot].npc, 0.5f);
                    DrawText("CLICK TO DEPLOY", (int)virtualMouse.x + 20, (int)virtualMouse.y, 10, GREEN);
                }

                if (p1->health <= 0) {
                    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.8f));
                    DrawText("YOU DIED", 340, 180, 20, RED);
                    DrawText("Open pause menu and press REVIVE.", 282, 208, 15, GRAY);
                }

                DrawInterface(p1, &draftNPC, virtualMouse);

                DrawGuideMenu(p1,
                              &wantsRestart,
                              &wantsReturnToMenu,
                              &wantsRevive,
                              &pauseShowAbout,
                              &pauseShowMore,
                              localIsHost,
                              net.enabled,
                              localIsHost);

                DrawMobileControlsOverlay(&mobileControls);

                if (net.enabled) {
                    int peers = CountConnectedPeers(&net);
                    const char *modeText = (net.mode == NET_HOST) ? "NET: HOST" : "NET: CLIENT";
                    DrawText(modeText, 610, 10, 10, SKYBLUE);

                    if (net.mode == NET_HOST) {
                        DrawText(TextFormat("PLAYERS: %d/%d", 1 + peers, MAX_PLAYERS), 610, 24, 10, GREEN);
                    } else {
                        DrawText(TextFormat("VISIBLE PEERS: %d", peers), 610, 24, 10, GREEN);
                    }

                    DrawText(TextFormat("WORLD SLOT: %d", selectedWorldSlot + 1), 610, 38, 10, LIGHTGRAY);
                } else {
                    DrawText(TextFormat("MY WORLD SLOT: %d", selectedWorldSlot + 1), 640, 10, 10, SKYBLUE);
                    DrawText("AUTO-SAVE ON", 680, 24, 10, SKYBLUE);
                }

#if defined(PLATFORM_ANDROID)
                DrawText("AUTO SAVE ACTIVE | ESC PAUSE (EXTERNAL INPUT) | MOBILE BUILD", 382, 436, 10, LIGHTGRAY);
#else
                DrawText("F5 SAVE | F9 MENU | ESC PAUSE | F11 FULLSCREEN", 460, 436, 10, LIGHTGRAY);
#endif
            }
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            Rectangle sourceRec = {0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height};
            Rectangle destRec = {offsetX, offsetY, (float)SCREEN_W * scale, (float)SCREEN_H * scale};
            DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    if (appScreen == APP_GAME) {
        bool localIsHost = (!net.enabled || net.mode == NET_HOST);
        if (localIsHost) SaveWorldState(activeWorldPath, &players[0]);
        SavePlayerProfile(activeProfilePath, &players[0]);
    }

    NetShutdown(&net);
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}