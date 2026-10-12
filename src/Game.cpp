//
//  Game.cpp
//  Misja (C++ / SDL3)
//
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/


#include "Game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
const char kSnapMagic[4] = {'M', 'S', 'J', '1'};
// Misja::mainLoop – miejsca oczekiwania, w których cały stan gry jest w pamięci
constexpr int kParkTimer = 1;      // $765D: CDTMV3
constexpr int kParkLine = 2;       // $7672: VCOUNT
}

Game::Game(const std::vector<uint8_t>& image, Pokey& pokey) : pokey_(pokey) {
    misja_ = std::make_unique<Misja>(machine_, image);
    machine_.onPokey = [this](int audf, int audc) { pokey_.write(audc >> 8, audf, audc & 0xFF); };
    applyInput();
}

Game::~Game() {
    machine_.restart();   // zatrzymanie wątku gry przed usunięciem Misja
    pokey_.silence();
}

void Game::frame() {
    machine_.frame();
    drawCredit();
}

// Napis pod ekranem tytułowym (jak w Robbo): czcionką tytułu oryginału, pod dolną linią.
// Tylko obraz – pamięć gry się nie zmienia.
void Game::drawCredit() {
    const auto& m = machine_.m;
    if (machine_.word(0x230) != 0x6826 || m[0x22F] == 0) return;
    static const char* text = "konwersja: leszek klich";
    const int len = static_cast<int>(std::strlen(text));
    const int firstRow = 220;                 // linia obrazu 228 (lista wyświetlania kończy się na 224)
    const int col0 = (32 - len) / 2;
    const auto& pal = Machine::palette();
    const uint32_t colors[4] = {pal[machine_.gtiaReg(0x1A)], pal[machine_.gtiaReg(0x16)], pal[machine_.gtiaReg(0x17)],
                                pal[0x0A]};
    auto& px = machine_.pixels();
    for (int i = 0; i < len; ++i) {
        // kod znaku jak w $6A39 (litery małe, reszta przesunięta o $20)
        int a = static_cast<unsigned char>(text[i]);
        if (a < 0x61) a += 0x20;
        if (a == 0x40) a = 0;
        for (int line = 0; line < 16; ++line) {
            const int base = line < 8 ? 0x5800 : 0x5C00;   // górna i dolna połowa znaku
            const uint8_t d = m[base + (a & 0x7F) * 8 + (line & 7)];
            uint32_t* out = px.data() + (firstRow + line) * width + 40 + (col0 + i) * 8;
            for (int p = 0; p < 4; ++p) {
                const uint32_t c = colors[(d >> (6 - p * 2)) & 3];
                out[p * 2] = out[p * 2 + 1] = c;
            }
        }
    }
}

void Game::setKey(GameKey key, bool pressed) {
    keys_[static_cast<int>(key)] = pressed;
    applyInput();
}

void Game::applyInput() {
    int stick = 0x0F;
    if (keys_[static_cast<int>(GameKey::up)]) stick &= ~1;
    const bool grenade = keys_[static_cast<int>(GameKey::grenade)];
    if (keys_[static_cast<int>(GameKey::down)] || grenade) stick &= ~2;
    if (keys_[static_cast<int>(GameKey::left)]) stick &= ~4;
    if (keys_[static_cast<int>(GameKey::right)]) stick &= ~8;
    machine_.setStick(stick);
    machine_.setTrig(keys_[static_cast<int>(GameKey::fire)] || grenade);
    int consol = 7;
    if (keys_[static_cast<int>(GameKey::start)]) consol &= ~1;
    if (keys_[static_cast<int>(GameKey::select)]) consol &= ~2;
    if (keys_[static_cast<int>(GameKey::option)]) consol &= ~4;
    machine_.setConsol(consol);
}

void Game::atariKeyDown(int kbcode) { machine_.keyDown(kbcode); }
void Game::atariKeyUp() { machine_.keyUp(); }
void Game::pressBreak() { machine_.pressBreak(); }

void Game::releaseAllKeys() {
    for (bool& k : keys_) k = false;
    applyInput();
    machine_.keyUp();
}

void Game::reset() {
    releaseAllKeys();
    pokey_.silence();
    Misja* m = misja_.get();
    machine_.restart([m] {
        m->restartState();
        m->start();
    });
}

void Game::setCheat(Cheat c, bool on) {
    switch (c) {
    case Cheat::energy: misja_->infiniteEnergy = on; break;
    case Cheat::ammo: misja_->infiniteAmmo = on; break;
    case Cheat::grenades: misja_->infiniteGrenades = on; break;
    }
}

bool Game::cheat(Cheat c) const {
    switch (c) {
    case Cheat::energy: return misja_->infiniteEnergy;
    case Cheat::ammo: return misja_->infiniteAmmo;
    case Cheat::grenades: return misja_->infiniteGrenades;
    }
    return false;
}

bool Game::isPlaying() const {
    const auto& m = machine_.m;
    return m[0x230] == 0x1C && m[0x231] == 0x74 && !(m[0xF2] & 0x80);
}

bool Game::canSave() const {
    return isPlaying() && (machine_.parkedAt(kParkTimer) || machine_.parkedAt(kParkLine));
}

Game::Info Game::info() const {
    const auto& m = machine_.m;
    Info i;
    i.room = m[0x89];
    char b[8];
    std::snprintf(b, sizeof(b), "%02X%02X%02X", m[0xF1], m[0xF0], m[0xEF]);
    i.score = b;
    i.energy = m[0xE4] * 100 / 0x77;
    i.containers = m[0xE9];
    return i;
}

bool Game::makeSnapshot(std::vector<uint8_t>& out) const {
    if (!canSave()) return false;
    const Machine::HwState hw = machine_.hwState();
    out.assign(kSnapMagic, kSnapMagic + 4);
    out.push_back(static_cast<uint8_t>(machine_.parkedAt(kParkLine) ? kParkLine : kParkTimer));
    out.insert(out.end(), machine_.m.begin(), machine_.m.end());
    out.insert(out.end(), hw.gtia.begin(), hw.gtia.end());
    out.insert(out.end(), hw.antic.begin(), hw.antic.end());
    out.insert(out.end(), hw.coll.begin(), hw.coll.end());
    out.insert(out.end(), hw.pia.begin(), hw.pia.end());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(hw.rnd >> (i * 8)));
    return true;
}

bool Game::restore(const std::vector<uint8_t>& s) {
    const size_t need = 5 + 0x10000 + 32 + 16 + 16 + 4 + 4;
    if (s.size() != need || std::memcmp(s.data(), kSnapMagic, 4) != 0) return false;
    const int point = s[4] == kParkLine ? kParkLine : kParkTimer;
    releaseAllKeys();
    pokey_.silence();
    Misja* m = misja_.get();
    machine_.restart([m, point] { m->resume(point); });
    size_t p = 5;
    std::copy(s.begin() + p, s.begin() + p + 0x10000, machine_.m.begin());
    p += 0x10000;
    Machine::HwState hw;
    std::copy(s.begin() + p, s.begin() + p + 32, hw.gtia.begin()); p += 32;
    std::copy(s.begin() + p, s.begin() + p + 16, hw.antic.begin()); p += 16;
    std::copy(s.begin() + p, s.begin() + p + 16, hw.coll.begin()); p += 16;
    std::copy(s.begin() + p, s.begin() + p + 4, hw.pia.begin()); p += 4;
    hw.rnd = 0;
    for (int i = 0; i < 4; ++i) hw.rnd |= uint32_t(s[p + i]) << (i * 8);
    machine_.setHwState(hw);
    return true;
}
