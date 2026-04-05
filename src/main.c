#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define NET_PORT 42042
#define NET_MAGIC 0x4D455453u
#define SAVE_FILE_PATH "magic_world/savegame.bin"

typedef enum { NET_DISABLED = 0, NET_HOST = 1, NET_CLIENT = 2 } NetMode;
typedef enum { APP_MAIN_MENU = 0, APP_MULTI_MENU = 1, APP_OTHERS_MENU = 2, APP_GAME = 3 } AppScreen;

typedef struct {
    uint32_t magic;
    uint32_t seq;
    float x;
    float y;
    float z;
    float health;
    float aimX;
    float aimY;
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
} NetPlayerPacket;

typedef struct {
    bool enabled;
    NetMode mode;
    int socketFd;
    struct sockaddr_in peerAddr;
    bool hasPeer;
    uint32_t seq;
    uint32_t lastRecvSeq;
} NetSession;

typedef struct {
    bool cast;
    Vector2 castTarget;
    float castChargeMult;
    float castLifeMult;
    bool spawnNpc;
    Vector2 npcSpawnPos;
} LocalNetEvents;

static Vector2 NormalizeSafe(Vector2 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len <= 0.0001f) return (Vector2){1.0f, 0.0f};
    return (Vector2){v.x / len, v.y / len};
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

static bool SaveExists(void) {
    FILE *fp = fopen(SAVE_FILE_PATH, "rb");
    if (!fp) return false;
    fclose(fp);
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
    p->aimDir = NormalizeSafe(p->aimDir);

    SetupPlayerCameras(p);
}

static void StartMyWorld(Player players[], bool tryLoadSave) {
    InitSimulation();
    InitNPCs();

    InitializePlayer(&players[0], 0, (Vector2){WORLD_W / 2.0f, WORLD_H / 2.0f});
    InitializePlayer(&players[1], 1, (Vector2){WORLD_W / 2.0f + 60.0f, WORLD_H / 2.0f + 30.0f});

    if (tryLoadSave) {
        Player loaded = players[0];
        if (LoadWorldState(SAVE_FILE_PATH, &loaded)) {
            players[0] = loaded;
            NormalizeLoadedPlayer(&players[0], 0);
        }
    }

    CopyPlayerLoadout(&players[1], &players[0]);
    players[1].health = players[1].maxHealth;
}

static bool NetInit(NetSession *net, NetMode mode, const char *joinIp) {
    memset(net, 0, sizeof(*net));
    net->enabled = (mode != NET_DISABLED);
    net->mode = mode;
    net->socketFd = -1;

    if (!net->enabled) return true;

    net->socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (net->socketFd < 0) {
        net->enabled = false;
        return false;
    }

    int flags = fcntl(net->socketFd, F_GETFL, 0);
    fcntl(net->socketFd, F_SETFL, flags | O_NONBLOCK);

    if (mode == NET_HOST) {
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

        memset(&net->peerAddr, 0, sizeof(net->peerAddr));
        net->peerAddr.sin_family = AF_INET;
        net->peerAddr.sin_port = htons(NET_PORT);
        net->peerAddr.sin_addr.s_addr = inet_addr(joinIp);
        net->hasPeer = true;
    }

    net->seq = 0;
    net->lastRecvSeq = 0;
    return true;
}

static void NetShutdown(NetSession *net) {
    if (net->socketFd >= 0) {
        close(net->socketFd);
        net->socketFd = -1;
    }
    net->enabled = false;
    net->hasPeer = false;
    net->mode = NET_DISABLED;
}

static void BuildPacket(const Player *local, const LocalNetEvents *ev, uint32_t seq, NetPlayerPacket *pkt) {
    pkt->magic = NET_MAGIC;
    pkt->seq = seq;
    pkt->x = local->pos.x;
    pkt->y = local->pos.y;
    pkt->z = local->z;
    pkt->health = local->health;
    pkt->aimX = local->aimDir.x;
    pkt->aimY = local->aimDir.y;
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
}

static void ApplyRemotePacket(Player *remote, const NetPlayerPacket *pkt) {
    remote->pos = (Vector2){pkt->x, pkt->y};
    remote->z = pkt->z;
    remote->health = pkt->health;
    remote->activeSlot = pkt->activeSlot;
    remote->castLayer = pkt->castLayer;
    remote->chargeLevel = pkt->chargeLevel;
    remote->lifespanLevel = pkt->lifespanLevel;
    remote->aimDir = NormalizeSafe((Vector2){pkt->aimX, pkt->aimY});
}

static void NetUpdate(NetSession *net, Player players[], LocalNetEvents *localEvents) {
    if (!net->enabled || net->socketFd < 0) return;

    NetPlayerPacket sendPkt;
    BuildPacket(&players[0], localEvents, ++net->seq, &sendPkt);
    if (net->hasPeer) {
        sendto(net->socketFd, &sendPkt, sizeof(sendPkt), 0, (struct sockaddr *)&net->peerAddr, sizeof(net->peerAddr));
    }

    while (1) {
        NetPlayerPacket recvPkt;
        struct sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        ssize_t readBytes = recvfrom(net->socketFd, &recvPkt, sizeof(recvPkt), 0, (struct sockaddr *)&fromAddr, &fromLen);
        if (readBytes < (ssize_t)sizeof(NetPlayerPacket)) break;
        if (recvPkt.magic != NET_MAGIC) continue;

        if (net->mode == NET_HOST && !net->hasPeer) {
            net->peerAddr = fromAddr;
            net->hasPeer = true;
        }

        if (net->mode == NET_HOST && net->hasPeer) {
            if (fromAddr.sin_addr.s_addr != net->peerAddr.sin_addr.s_addr || fromAddr.sin_port != net->peerAddr.sin_port) continue;
        }

        if (recvPkt.seq <= net->lastRecvSeq) continue;
        net->lastRecvSeq = recvPkt.seq;

        ApplyRemotePacket(&players[1], &recvPkt);

        if (recvPkt.castEvent && players[1].hotbar[players[1].activeSlot].type == ITEM_SPELL) {
            ExecuteSpell(&players[1], (Vector2){recvPkt.castTargetX, recvPkt.castTargetY},
                         &players[1].hotbar[players[1].activeSlot].spell,
                         recvPkt.castChargeMult, recvPkt.castLifeMult);
        }

        if (recvPkt.spawnNpcEvent && players[1].hotbar[players[1].activeSlot].type == ITEM_NPC) {
            SpawnNPC((Vector2){recvPkt.npcSpawnX, recvPkt.npcSpawnY}, players[1].hotbar[players[1].activeSlot].npc);
        }
    }
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
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "Metsys: Open World Ecosystem");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    RenderTexture2D target = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    Player players[MAX_PLAYERS];
    InitializePlayer(&players[0], 0, (Vector2){WORLD_W / 2.0f, WORLD_H / 2.0f});
    InitializePlayer(&players[1], 1, (Vector2){WORLD_W / 2.0f + 60.0f, WORLD_H / 2.0f + 30.0f});

    NetSession net;
    memset(&net, 0, sizeof(net));
    net.socketFd = -1;

    AppScreen appScreen = APP_MAIN_MENU;
    NPCDNA draftNPC = { 50, 0, 0, 50, 50, 0 };
    bool wantsRestart = false;
    float autosaveTimer = 0.0f;

    char joinAddress[64] = "127.0.0.1";
    bool editingJoinAddress = false;

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

        Player *p1 = &players[0];
        Player *p2 = &players[1];
        LocalNetEvents localEvents = {0};

        if (appScreen == APP_GAME) {
            if (IsKeyPressed(KEY_F9)) {
                if (!net.enabled) SaveWorldState(SAVE_FILE_PATH, p1);
                NetShutdown(&net);
                appScreen = APP_MAIN_MENU;
                continue;
            }

            if (IsKeyPressed(KEY_F5) && !net.enabled) {
                SaveWorldState(SAVE_FILE_PATH, p1);
            }

            if (wantsRestart) {
                ResetGame(p1);
                wantsRestart = false;
            }

            if (IsKeyPressed(KEY_ESCAPE)) p1->showGuide = !p1->showGuide;
            if (IsKeyPressed(KEY_GRAVE) && !p1->showGuide && p1->health > 0) {
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

            if (IsKeyPressed(KEY_LEFT_SHIFT)) p1->castLayer = !p1->castLayer;
            if (IsKeyDown(KEY_RIGHT_BRACKET)) p1->visionBlend = fminf(2.0f, p1->visionBlend + dt * 1.5f);
            if (IsKeyDown(KEY_LEFT_BRACKET)) p1->visionBlend = fmaxf(0.0f, p1->visionBlend - dt * 1.5f);

            if (!p1->isCrafting && !p1->showGuide && p1->health > 0) {
                if (IsKeyPressed(KEY_SPACE) && !p1->isJumping) {
                    p1->isJumping = true;
                    p1->zVelocity = 250.0f;
                }

                Vector2 delta = {0};
                if (IsKeyDown(KEY_W)) delta.y -= p1->speed * dt;
                if (IsKeyDown(KEY_S)) delta.y += p1->speed * dt;
                if (IsKeyDown(KEY_A)) delta.x -= p1->speed * dt;
                if (IsKeyDown(KEY_D)) delta.x += p1->speed * dt;
                MovePlayer(p1, delta, dt);

                for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) p1->activeSlot = i;
                if (IsKeyPressed(KEY_ZERO)) p1->activeSlot = 9;

                HotbarSlot *active = &p1->hotbar[p1->activeSlot];
                Vector2 worldMouse = GetScreenToWorld2D(virtualMouse, p1->worldCamera);
                p1->aimDir = NormalizeSafe((Vector2){worldMouse.x - p1->pos.x, worldMouse.y - p1->pos.y});

                if (active->type == ITEM_SPELL) {
                    bool holdingLMB = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
                    bool holdingC = IsKeyDown(KEY_C);

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

                    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                        float chargeMult = 1.0f + p1->chargeLevel;
                        float lifeMult = 1.0f + p1->lifespanLevel;
                        ExecuteSpell(p1, worldMouse, &active->spell, chargeMult, lifeMult);

                        localEvents.cast = true;
                        localEvents.castTarget = worldMouse;
                        localEvents.castChargeMult = chargeMult;
                        localEvents.castLifeMult = lifeMult;

                        p1->chargeLevel = 0.0f;
                        p1->lifespanLevel = 0.0f;
                    }
                } else if (active->type == ITEM_NPC) {
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        SpawnNPC(worldMouse, active->npc);
                        localEvents.spawnNpc = true;
                        localEvents.npcSpawnPos = worldMouse;
                    }
                }
            }

            if (net.enabled) NetUpdate(&net, players, &localEvents);

            int simPlayerCount = net.enabled ? 2 : 1;

            Vector2 camTarget = p1->pos;
            if (net.enabled && p2->health > 0.0f) {
                camTarget.x = (p1->pos.x + p2->pos.x) * 0.5f;
                camTarget.y = (p1->pos.y + p2->pos.y) * 0.5f;
            }
            p1->worldCamera.target.x += (camTarget.x - p1->worldCamera.target.x) * 5.0f * dt;
            p1->worldCamera.target.y += (camTarget.y - p1->worldCamera.target.y) * 5.0f * dt;

            UpdateSimulation(dt, players, simPlayerCount);
            UpdateNPCs(dt, players, simPlayerCount);

            if (!net.enabled) {
                autosaveTimer += dt;
                if (autosaveTimer > 6.0f) {
                    SaveWorldState(SAVE_FILE_PATH, p1);
                    autosaveTimer = 0.0f;
                }
            } else {
                autosaveTimer = 0.0f;
            }
        }

        BeginTextureMode(target);
            ClearBackground((Color){20, 20, 25, 255});

            if (appScreen == APP_MAIN_MENU) {
                DrawMenuBackdrop(GetTime());
                DrawText("METSYS ENGINE", 258, 70, 42, GOLD);
                DrawText("Temporal Graph Spellcraft Sandbox", 260, 118, 16, SKYBLUE);

                Rectangle myWorldRec = {280, 170, 240, 45};
                Rectangle multiRec = {280, 225, 240, 45};
                Rectangle othersRec = {280, 280, 240, 45};
                Rectangle exitRec = {280, 335, 240, 45};

                if (DrawMenuButton(myWorldRec, "MY WORLD", virtualMouse)) {
                    NetShutdown(&net);
                    StartMyWorld(players, true);
                    wantsRestart = false;
                    autosaveTimer = 0.0f;
                    appScreen = APP_GAME;
                }
                if (DrawMenuButton(multiRec, "MULTIPLAYER", virtualMouse)) {
                    appScreen = APP_MULTI_MENU;
                }
                if (DrawMenuButton(othersRec, "OTHERS", virtualMouse)) {
                    appScreen = APP_OTHERS_MENU;
                }
                if (DrawMenuButton(exitRec, "EXIT", virtualMouse)) {
                    EndTextureMode();
                    break;
                }

                DrawText(SaveExists() ? "SAVE DATA: FOUND" : "SAVE DATA: NONE", 20, 420, 10, LIGHTGRAY);
            }

            if (appScreen == APP_MULTI_MENU) {
                DrawMenuBackdrop(GetTime() * 0.8f);
                DrawText("MULTIPLAYER", 300, 65, 36, GOLD);
                DrawText("HOST OR JOIN A SESSION", 276, 105, 14, SKYBLUE);

                Rectangle hostRec = {280, 160, 240, 45};
                Rectangle joinRec = {280, 215, 240, 45};
                Rectangle backRec = {280, 350, 240, 40};
                Rectangle inputRec = {250, 282, 300, 32};

                UpdateJoinAddressInput(joinAddress, (int)sizeof(joinAddress), &editingJoinAddress, inputRec, virtualMouse);

                DrawRectangleRec(inputRec, Fade(BLACK, 0.8f));
                DrawRectangleLinesEx(inputRec, 2.0f, editingJoinAddress ? GOLD : GRAY);
                DrawText("JOIN ADDRESS", 250, 264, 10, LIGHTGRAY);
                DrawText(joinAddress, 258, 291, 18, RAYWHITE);

                if (DrawMenuButton(hostRec, "HOST SESSION", virtualMouse)) {
                    NetShutdown(&net);
                    if (NetInit(&net, NET_HOST, joinAddress)) {
                        StartMyWorld(players, true);
                        wantsRestart = false;
                        appScreen = APP_GAME;
                    }
                }
                if (DrawMenuButton(joinRec, "JOIN SESSION", virtualMouse)) {
                    NetShutdown(&net);
                    if (NetInit(&net, NET_CLIENT, joinAddress)) {
                        StartMyWorld(players, false);
                        wantsRestart = false;
                        appScreen = APP_GAME;
                    }
                }
                if (DrawMenuButton(backRec, "BACK", virtualMouse)) {
                    appScreen = APP_MAIN_MENU;
                }

                DrawText("TIP: HOST USES WORLD SAVE. CLIENT USES NETWORK STATE.", 190, 410, 10, LIGHTGRAY);
            }

            if (appScreen == APP_OTHERS_MENU) {
                DrawMenuBackdrop(GetTime() * 0.6f);
                DrawText("OTHERS", 335, 65, 36, GOLD);

                DrawRectangle(90, 115, 620, 245, Fade(BLACK, 0.8f));
                DrawRectangleLines(90, 115, 620, 245, SKYBLUE);
                DrawText("COMPENDIUM SNAPSHOT", 110, 130, 16, SKYBLUE);
                DrawText("SPD speed | DLY delay | SPR spread | DST distortion | RNG range | SIZ size", 110, 158, 10, RAYWHITE);
                DrawText("COND ALW always | T> time gate | S>/S< scalar thresholds", 110, 174, 10, RAYWHITE);
                DrawText("CH TMP/MAS/WET/COH/CHG chooses scalar channel", 110, 190, 10, RAYWHITE);
                DrawText("TOOLS BLD DIG WET DRY HOT COO CON BHO WHO", 110, 206, 10, RAYWHITE);
                DrawText("IN GAME: OPEN CRAFT, THEN PRESS F1 FOR FULL COMPENDIUM", 110, 222, 10, GOLD);
                DrawText("KEYS: F11 fullscreen | F5 save | F9 return to menu", 110, 238, 10, LIGHTGRAY);

                Rectangle eraseRec = {260, 300, 280, 38};
                if (DrawMenuButton(eraseRec, "ERASE LOCAL SAVE", virtualMouse)) {
                    remove(SAVE_FILE_PATH);
                }

                Rectangle backRec = {300, 362, 200, 36};
                if (DrawMenuButton(backRec, "BACK", virtualMouse)) appScreen = APP_MAIN_MENU;
            }

            if (appScreen == APP_GAME) {
                float matAlpha = 1.0f - (fminf(p1->visionBlend, 2.0f) * 0.4f);
                float nrgAlpha = 1.0f - fabsf(p1->visionBlend - 1.0f);
                float hazAlpha = fmaxf(0.0f, p1->visionBlend - 1.0f);

                BeginMode2D(p1->worldCamera);
                    DrawMaterialRealm(matAlpha);
                    if (nrgAlpha > 0.01f) DrawEnergyRealm(nrgAlpha);
                    if (hazAlpha > 0.01f) DrawHazardRealm(hazAlpha);

                    DrawSingularities(1.0f);
                    DrawNPCs();
                    DrawProjectiles(p1);

                    DrawPlayerEntity(p1);
                    if (net.enabled) {
                        DrawPlayerEntity(p2);
                        DrawCircleLines(p2->pos.x, p2->pos.y - p2->z, 10.0f, YELLOW);
                        DrawLineEx((Vector2){p2->pos.x, p2->pos.y - p2->z},
                                   (Vector2){p2->pos.x + p2->aimDir.x * 20.0f, p2->pos.y - p2->z + p2->aimDir.y * 20.0f},
                                   1.5f, YELLOW);
                    }
                EndMode2D();

                if (!p1->isCrafting && !p1->showGuide && p1->health > 0 && p1->hotbar[p1->activeSlot].type == ITEM_NPC) {
                    DrawProceduralNPC(virtualMouse, 0, p1->hotbar[p1->activeSlot].npc, 0.5f);
                    DrawText("CLICK TO DEPLOY", virtualMouse.x + 20, virtualMouse.y, 10, GREEN);
                }

                if (p1->health <= 0) {
                    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.8f));
                    DrawText("YOU DIED TO THE ELEMENTS", 240, 180, 20, RED);
                    DrawText("Press [ ` ] to configure spells before restarting.", 230, 210, 15, GRAY);
                }

                DrawInterface(p1, &draftNPC, virtualMouse);
                DrawGuideMenu(p1, &wantsRestart);

                if (net.enabled) {
                    const char *mode = (net.mode == NET_HOST) ? "NET: HOST" : "NET: CLIENT";
                    Color c = net.hasPeer ? GREEN : ORANGE;
                    DrawText(mode, 620, 10, 10, c);
                    DrawText(net.hasPeer ? "PEER LINKED" : "WAITING FOR PEER", 620, 24, 10, c);
                } else {
                    DrawText("MY WORLD", 680, 10, 10, SKYBLUE);
                    DrawText("AUTO-SAVE ON", 680, 24, 10, SKYBLUE);
                }

                DrawText("F5 SAVE | F9 MENU | F11 FULLSCREEN", 540, 436, 10, LIGHTGRAY);
            }

        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            Rectangle sourceRec = { 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height };
            Rectangle destRec = {
                offsetX,
                offsetY,
                (float)SCREEN_W * scale,
                (float)SCREEN_H * scale
            };
            DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    if (appScreen == APP_GAME && !net.enabled) SaveWorldState(SAVE_FILE_PATH, &players[0]);
    NetShutdown(&net);
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
