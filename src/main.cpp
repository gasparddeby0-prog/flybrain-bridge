// Fly Brain Bridge : relie Geometry Dash au script Python du cerveau de mouche.
// - envoie à 127.0.0.1:47800, à chaque image : "S <pourcent> <mort 0/1> <x> <y> <tentative> <au sol 0/1>"
// - écoute 127.0.0.1:47801 : "P1" = appuyer (saut), "P0" = relâcher
// Les appuis passent directement par le jeu : ça marche même si la fenêtre n'est pas au premier plan.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    SOCKET g_sock = INVALID_SOCKET;
    sockaddr_in g_dst{};
    bool g_down = false;

    void initSocket() {
        if (g_sock != INVALID_SOCKET) return;
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
        g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_sock == INVALID_SOCKET) return;
        sockaddr_in me{};
        me.sin_family = AF_INET;
        me.sin_port = htons(47801);
        me.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(g_sock, reinterpret_cast<sockaddr*>(&me), sizeof(me)) != 0) {
            log::warn("Fly Brain Bridge : port 47801 déjà pris");
        }
        u_long nonBlocking = 1;
        ioctlsocket(g_sock, FIONBIO, &nonBlocking);
        g_dst = me;
        g_dst.sin_port = htons(47800);
        log::info("Fly Brain Bridge prêt (UDP 47800/47801)");
    }

    void sendText(std::string const& txt) {
        if (g_sock == INVALID_SOCKET) return;
        sendto(g_sock, txt.c_str(), static_cast<int>(txt.size()), 0,
               reinterpret_cast<sockaddr*>(&g_dst), sizeof(g_dst));
    }
}

class $modify(FlyBrainPlayLayer, PlayLayer) {
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        initSocket();
        if (g_sock == INVALID_SOCKET || !m_player1) return;

        // 1. ordres du cerveau
        char buf[64];
        while (true) {
            int n = recv(g_sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = 0;
            if (buf[0] == 'P') {
                bool down = buf[1] == '1';
                if (down != g_down && !m_player1->m_isDead) {
                    this->handleButton(down, 1, true);
                    g_down = down;
                }
            }
        }
        if (m_player1->m_isDead && g_down) {
            this->handleButton(false, 1, true);
            g_down = false;
        }

        // 2. état du jeu
        sendText(fmt::format("S {:.3f} {} {:.2f} {:.2f} {} {}",
            this->getCurrentPercent(),
            m_player1->m_isDead ? 1 : 0,
            m_player1->getPositionX(),
            m_player1->getPositionY(),
            m_attempts,
            m_player1->m_isOnGround ? 1 : 0));
    }

    void onQuit() {
        if (g_down && m_player1) this->handleButton(false, 1, true);
        g_down = false;
        sendText("Q");
        PlayLayer::onQuit();
    }
};
