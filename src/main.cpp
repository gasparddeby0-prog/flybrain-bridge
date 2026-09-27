// Fly Brain Bridge : relie Geometry Dash au script Python du cerveau de mouche.
// - envoie à 127.0.0.1:47800, à chaque image : "S <pourcent> <mort 0/1> <x> <y> <tentative> <au sol 0/1>"
// - écoute 127.0.0.1:47801 : "P1" = appuyer (saut), "P0" = relâcher
// Les appuis passent directement par le jeu : ça marche même si la fenêtre n'est pas au premier plan.
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "net.hpp"

using namespace geode::prelude;

namespace {
    bool g_ready = false;
    bool g_down = false;

    void sendText(std::string const& txt) {
        net_send(txt.c_str(), static_cast<int>(txt.size()));
    }
}

class $modify(FlyBrainPlayLayer, PlayLayer) {
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!g_ready) {
            g_ready = net_init();
            if (g_ready) log::info("Fly Brain Bridge prêt (UDP 47800/47801)");
        }
        if (!g_ready || !m_player1) return;

        // 1. ordres du cerveau
        char buf[64];
        while (true) {
            int n = net_recv(buf, sizeof(buf) - 1);
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
