// Fly Brain Bridge : relie Geometry Dash au script Python du cerveau de mouche.
// - envoie à 127.0.0.1:47800, à chaque image :
//   "S <pourcent> <mort> <x> <y> <tentative> <au sol> <vaisseau> <vitesse verticale> L <d1> <t1> ... <d6> <t6>"
//   L = 6 lasers horizontaux devant le cube : distance (unités du jeu, 30 = 1 bloc) et type touché
//   (0 rien, 1 pic/danger, 2 bloc solide). Portée 300 unités (10 blocs).
// - écoute 127.0.0.1:47801 : "P1" = appuyer (saut), "P0" = relâcher,
//   "M1"/"M0" = mode entraînement (practice) avec checkpoints automatiques, "C0" = enlever le dernier checkpoint
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "net.hpp"

using namespace geode::prelude;

namespace {
    bool g_ready = false;
    bool g_down = false;
    bool g_autoCheckpoints = false;
    float g_lastCheckpointX = -1e9f;
    constexpr float CHECKPOINT_EVERY = 240.f;   // un checkpoint tous les 8 blocs environ

    // hauteurs des lasers par rapport au bas du cube (unités du jeu)
    constexpr float LASER_DY[6] = {2.f, 14.f, 28.f, 45.f, 65.f, 95.f};
    constexpr float RANGE = 300.f;

    void sendText(std::string const& txt) {
        net_send(txt.c_str(), static_cast<int>(txt.size()));
    }
}

class $modify(FlyBrainPlayLayer, PlayLayer) {
    std::string lasers() {
        float dist[6];
        int kind[6];
        for (int i = 0; i < 6; i++) { dist[i] = RANGE; kind[i] = 0; }

        auto player = m_player1;
        auto prect = player->getObjectRect();
        float px = prect.getMaxX();                  // les lasers partent de l'avant du cube
        float py = prect.getMinY();

        if (m_objects) {
            for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                if (!obj) continue;
                int k = 0;
                if (obj->m_objectType == GameObjectType::Hazard) k = 1;
                else if (obj->m_objectType == GameObjectType::Solid) k = 2;
                else continue;
                auto r = obj->getObjectRect();
                if (r.size.width <= 0 || r.size.height <= 0) continue;
                float d = r.getMinX() - px;
                if (r.getMaxX() < px || d > RANGE) continue;
                if (d < 0) d = 0;                    // objet déjà à hauteur du cube
                for (int i = 0; i < 6; i++) {
                    float y = py + LASER_DY[i];
                    if (y >= r.getMinY() && y <= r.getMaxY() && d < dist[i]) {
                        dist[i] = d;
                        kind[i] = k;
                    }
                }
            }
        }
        std::string s = " L";
        for (int i = 0; i < 6; i++) s += fmt::format(" {:.1f} {}", dist[i], kind[i]);
        return s;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!g_ready) {
            g_ready = net_init();
            if (g_ready) log::info("Fly Brain Bridge prêt (UDP 47800/47801), lasers actifs");
        }
        if (!g_ready || !m_player1) return;

        // 1. ordres du cerveau
        char buf[64];
        while (true) {
            int n = net_recv(buf, sizeof(buf) - 1);
            if (n <= 0) break;
            buf[n] = 0;
            if (buf[0] == 'M') {
                g_autoCheckpoints = buf[1] == '1';
                if (m_isPracticeMode != g_autoCheckpoints) this->togglePracticeMode(g_autoCheckpoints);
                g_lastCheckpointX = m_player1->getPositionX();
            } else if (buf[0] == 'C' && m_isPracticeMode) {
                this->removeCheckpoint(false);
                g_lastCheckpointX = m_player1->getPositionX();
            } else if (buf[0] == 'P') {
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

        // checkpoint automatique : au sol, en vie, rien de dangereux juste devant
        if (g_autoCheckpoints && m_isPracticeMode && !m_player1->m_isDead && m_player1->m_isOnGround
            && !m_player1->m_isShip && m_player1->getPositionX() - g_lastCheckpointX > CHECKPOINT_EVERY) {
            auto s = lasers();
            // lasers : " L d1 t1 d2 t2 ..." ; on exige qu'aucun obstacle ne soit à moins de 6 blocs
            bool clear = true;
            float d; int t; const char* p = s.c_str() + 3;
            for (int i = 0; i < 6; i++) {
                if (sscanf(p, "%f %d", &d, &t) != 2) break;
                if (t != 0 && d < 180.f) clear = false;
                for (int sp = 0; sp < 2 && *p; ) { if (*p == ' ') sp++; p++; if (sp == 2) break; }
            }
            if (clear) {
                this->markCheckpoint();
                g_lastCheckpointX = m_player1->getPositionX();
            }
        }

        // 2. état du jeu + lasers
        sendText(fmt::format("S {:.3f} {} {:.2f} {:.2f} {} {} {} {:.2f} {}",
            this->getCurrentPercent(),
            m_player1->m_isDead ? 1 : 0,
            m_player1->getPositionX(),
            m_player1->getPositionY(),
            m_attempts,
            m_player1->m_isOnGround ? 1 : 0,
            m_player1->m_isShip ? 1 : 0,
            static_cast<float>(m_player1->m_yVelocity),
            m_isPracticeMode ? 1 : 0) + lasers());
    }

    void onQuit() {
        if (g_down && m_player1) this->handleButton(false, 1, true);
        g_down = false;
        sendText("Q");
        PlayLayer::onQuit();
    }
};
