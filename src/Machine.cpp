//
//  Machine.cpp
//  Misja (C++ / SDL3)
//

#include "Machine.h"

#include <algorithm>
#include <cstring>

namespace {

const uint8_t kPaletteRGB[768] = {
#include "Palette.inc"
};

// Wysokość linii trybu ANTIC (w liniach obrazu) i liczba bajtów w zwykłej szerokości (40 znaków).
const int kModeHeight[16] = {1, 1, 8, 10, 8, 16, 8, 16, 8, 4, 4, 2, 1, 2, 1, 1};
const int kModeBytes[16] = {0, 0, 40, 40, 40, 40, 20, 20, 10, 10, 20, 20, 20, 40, 40, 40};

int modeBytes(int mode, int width) {
    const int n = kModeBytes[mode];
    if (width == 1) return n * 4 / 5;   // wąskie pole (256 punktów)
    if (width == 3) return n * 6 / 5;   // szerokie pole
    return n;
}

}  // namespace

const std::vector<uint32_t>& Machine::palette() {
    static const std::vector<uint32_t> pal = [] {
        std::vector<uint32_t> p(256);
        for (int i = 0; i < 256; ++i) {
            p[i] = 0xFF000000u | (uint32_t(kPaletteRGB[i * 3]) << 16) | (uint32_t(kPaletteRGB[i * 3 + 1]) << 8) |
                   kPaletteRGB[i * 3 + 2];
        }
        return p;
    }();
    return pal;
}

Machine::Machine() : pixels_(width * height, 0xFF000000u) {
    // stan po starcie systemu (ROM Atari)
    m[0x222] = 0x5F; m[0x223] = 0xE4;   // VVBLKI = SYSVBV
    m[0x224] = 0x62; m[0x225] = 0xE4;   // VVBLKD = XITVBV
    m[0x2F4] = 0xE0;                     // CHBAS
    m[0x22F] = 0x22;                     // SDMCTL
    m[0x2FC] = 0xFF;                     // CH – brak klawisza
    m[0x11] = 0xFF;                      // BRKKEY
    m[0x6A] = 0xC0;                      // RAMTOP
    m[0x300] = 0x60;                     // DDEVIC – gra wczytana z kasety (sprawdza to zabezpieczenie)
    m[0x240] = 0x9B;                     // DFLAGS – pierwszy bajt bloku startowego kasety (też sprawdzany)
    antic_[0x0E] = 0x40;                 // NMIEN: VBI
}

Machine::~Machine() { stopThread(); }

// MARK: - Rejestry

void Machine::gtia(int reg, int v) {
    reg &= 0x1F;
    if (reg == 0x1E) { hitclr(); return; }
    if (inDli_) { pending_.push_back({0xD000 + reg, v}); return; }
    gtia_[reg] = static_cast<uint8_t>(v);
}

void Machine::hitclr() { coll_.fill(0); }

void Machine::antic(int reg, int v) {
    reg &= 0x0F;
    if (reg == 0x0A) return;   // WSYNC – kod gry wykonuje się w chwili, więc nie ma na co czekać
    if (reg == 0x0F) return;   // NMIRES
    if (inDli_) { pending_.push_back({0xD400 + reg, v}); return; }
    antic_[reg] = static_cast<uint8_t>(v);
}

void Machine::pokey(int reg, int v) {
    reg &= 0x0F;
    if (reg < 8 && onPokey) {
        const int ch = reg >> 1;
        if (reg & 1) audc_[ch] = static_cast<uint8_t>(v); else audf_[ch] = static_cast<uint8_t>(v);
        onPokey(audf_[ch], audc_[ch] | (ch << 8));
    }
}

void Machine::pia(int reg, int v) {
    reg &= 3;
    if (reg == 0) {
        if (pia_[2] & 4) pia_[0] = static_cast<uint8_t>(v); else pia_[1] = static_cast<uint8_t>(v);
    } else if (reg == 2) {
        pia_[2] = static_cast<uint8_t>(v);
    } else if (reg == 3) {
        pia_[3] = static_cast<uint8_t>(v);
    }
}

int Machine::piaRead(int reg) const {
    reg &= 3;
    if (reg == 0) return (pia_[2] & 4) ? (0xF0 | stick_) : pia_[1];
    if (reg == 2) return pia_[2];
    if (reg == 3) return pia_[3];
    return 0xFF;
}

int Machine::trig(int n) const { return (n == 0 && trigger_) ? 0 : 1; }
int Machine::consol() const { return consol_; }

int Machine::random() {
    // 17-bitowy rejestr przesuwny POKEY, 8 kroków na odczyt
    for (int i = 0; i < 8; ++i) {
        const uint32_t bit = (rnd_ ^ (rnd_ >> 5)) & 1;
        rnd_ = (rnd_ >> 1) | (bit << 16);
    }
    return static_cast<int>(rnd_ & 0xFF);
}

void Machine::keyDown(int code) {
    key_ = code & 0xFF;
    m[0x2FC] = static_cast<uint8_t>(code);   // CH (procedura przerwania klawiatury w ROM)
    m[0x4D] = 0;                             // ATRACT
}

void Machine::keyUp() { key_ = -1; }

void Machine::pressBreak() {
    m[0x11] = 0;      // BRKKEY
    m[0x4D] = 0;
}

Machine::HwState Machine::hwState() const {
    HwState s;
    s.gtia = gtia_;
    s.antic = antic_;
    s.coll = coll_;
    s.rnd = rnd_;
    s.pia = pia_;
    return s;
}

void Machine::setHwState(const HwState& s) {
    gtia_ = s.gtia;
    antic_ = s.antic;
    coll_ = s.coll;
    rnd_ = s.rnd;
    pia_ = s.pia;
}

// MARK: - Przerwanie VBI (ROM: SYSVBV, potem VVBLKD)

void Machine::vbi() {
    // etap 1
    if (++m[0x14] == 0) {
        m[0x4D]++;
        if (++m[0x13] == 0) m[0x12]++;
    }
    uint8_t drkmsk = 0xFE, colrsh = 0;
    if (m[0x4D] & 0x80) { m[0x4D] = 0xFE; colrsh = m[0x13]; drkmsk = 0xF6; }
    m[0x4E] = drkmsk;
    m[0x4F] = colrsh;
    antic_[2] = m[0x230];
    antic_[3] = m[0x231];
    antic_[0] = m[0x22F];
    gtia_[0x1B] = m[0x26F];
    // CDTMV1 (gra go nie używa)
    if (int t = word(0x218)) setWord(0x218, t - 1);
    if (m[0x42]) return;   // CRITIC – bez etapu 2
    // etap 2
    antic_[9] = m[0x2F4];
    antic_[1] = m[0x2F3];
    for (int i = 0; i < 9; ++i) gtia_[0x12 + i] = static_cast<uint8_t>((m[0x2C0 + i] ^ colrsh) & drkmsk);
    for (int k = 0; k < 4; ++k) {   // CDTMV2-CDTMV5
        const int adr = 0x21A + k * 2;
        if (int t = word(adr)) {
            setWord(adr, t - 1);
            if (t == 1 && k > 0) m[k == 1 ? 0x22A : k == 2 ? 0x22C : 0x22E] = 0;
        }
    }
    if (m[0x2F1]) m[0x2F1]--;
    m[0x278] = static_cast<uint8_t>(stick_);
    m[0x279] = m[0x27A] = m[0x27B] = 0x0F;
    m[0x284] = trigger_ ? 0 : 1;
    m[0x285] = m[0x286] = m[0x287] = 1;
    if (deferredVBI) deferredVBI();
}

// MARK: - Ramka

void Machine::anticLine() {
    if (line_ < 8 || line_ >= 248) { mode_ = -1; return; }
    if (modeLeft_ > 0) return;
    modeLine_ = 0;
    if (dlWait_) { mode_ = 0; modeLeft_ = 1; modeDLI_ = false; return; }
    const int ins = m[dlpc_];
    dlpc_ = (dlpc_ & 0xFC00) | ((dlpc_ + 1) & 0x3FF);
    modeDLI_ = (ins & 0x80) != 0;
    const int mode = ins & 0x0F;
    if (mode == 0) {                       // puste linie
        mode_ = 0;
        modeLeft_ = ((ins >> 4) & 7) + 1;
    } else if (mode == 1) {                // skok (JMP / JVB)
        dlpc_ = word(dlpc_);
        mode_ = 0;
        modeLeft_ = 1;
        if (ins & 0x40) dlWait_ = true;
    } else {
        if (ins & 0x40) {                  // LMS
            memscan_ = word(dlpc_);
            dlpc_ = (dlpc_ + 2) & 0xFFFF;
        }
        mode_ = mode;
        modeLeft_ = kModeHeight[mode];
        lineMemscan_ = memscan_;
        memscan_ = (memscan_ & 0xF000) | ((memscan_ + modeBytes(mode, antic_[0] & 3)) & 0x0FFF);
    }
}

void Machine::frame() {
    for (line_ = 0; line_ < 312; ++line_) {
        anticLine();
        if (line_ == 248) {
            if (antic_[0x0E] & 0x40) vbi();
        } else if (line_ >= 8 && line_ < 248 && modeDLI_ && modeLeft_ == 1) {
            if ((antic_[0x0E] & 0x80) && dli) {
                inDli_ = true;
                dli();
                inDli_ = false;
            }
        }
        runLogic();
        renderLine();
        // zmiany rejestrów z DLI działają od następnej linii (na Atari wpadają w trakcie ostatniej)
        for (const auto& w : pending_) {
            if (w.first < 0xD400) gtia_[w.first - 0xD000] = static_cast<uint8_t>(w.second);
            else antic_[w.first - 0xD400] = static_cast<uint8_t>(w.second);
        }
        pending_.clear();
        if (line_ >= 8 && line_ < 248 && modeLeft_ > 0) { modeLeft_--; modeLine_++; }
    }
    line_ = 0;
    dlWait_ = false;
    modeLeft_ = 0;
    dlpc_ = antic_[2] | (antic_[3] << 8);
    frames_++;
}

void Machine::renderLine() {
    if (line_ < 8 || line_ >= 248) return;
    const int row = line_ - 8;
    const auto& pal = palette();
    const uint8_t* g = gtia_.data();
    const int pfWidth = antic_[0] & 3;
    int mode = mode_;
    if (!(antic_[0] & 0x20) || pfWidth == 0) mode = 0;
    const int pfStart = pfWidth == 1 ? 64 : pfWidth == 2 ? 48 : 32;
    const int pfEnd = pfWidth == 1 ? 192 : pfWidth == 2 ? 208 : 224;
    const int chb = antic_[9];

    // grafika graczy i pocisków dla tej linii (DMA z PMBASE)
    uint8_t grafp[4], grafm;
    const int base = antic_[7] << 8;
    const bool single = (antic_[0] & 0x10) != 0;
    for (int i = 0; i < 4; ++i) {
        if ((gtia_[0x1D] & 2) && (antic_[0] & 8)) {
            grafp[i] = single ? m[(base & 0xF800) + 0x400 + i * 0x100 + line_]
                              : m[(base & 0xFC00) + 0x200 + i * 0x80 + (line_ >> 1)];
        } else {
            grafp[i] = gtia_[0x0D + i];
        }
    }
    if ((gtia_[0x1D] & 1) && (antic_[0] & 4)) {
        grafm = single ? m[(base & 0xF800) + 0x300 + line_] : m[(base & 0xFC00) + 0x180 + (line_ >> 1)];
    } else {
        grafm = gtia_[0x11];
    }

    uint32_t* out = pixels_.data() + row * width;
    for (int cc = 44; cc < 212; ++cc) {
        int pf = 0;              // 0 tło, 1-4 PF0-PF3
        bool hires = false;
        int hiresBits = 0;
        if (mode > 1 && cc >= pfStart && cc < pfEnd) {
            const int pos = cc - pfStart;
            switch (mode) {
            case 2: case 3: {
                const int ch = m[(lineMemscan_ + pos / 4) & 0xFFFF];
                uint8_t d = m[((chb & 0xFC) << 8) + (ch & 0x7F) * 8 + (modeLine_ & 7)];
                if (ch & 0x80) d = static_cast<uint8_t>(~d);
                hires = true;
                hiresBits = (d >> (6 - (pos % 4) * 2)) & 3;
                pf = 3;
                break;
            }
            case 4: case 5: {
                const int ch = m[(lineMemscan_ + pos / 4) & 0xFFFF];
                const int l = mode == 5 ? modeLine_ >> 1 : modeLine_;
                const uint8_t d = m[((chb & 0xFC) << 8) + (ch & 0x7F) * 8 + l];
                const int v = (d >> (6 - (pos % 4) * 2)) & 3;
                pf = (v == 3 && (ch & 0x80)) ? 4 : v;
                break;
            }
            case 6: case 7: {
                const int ch = m[(lineMemscan_ + pos / 8) & 0xFFFF];
                const int l = mode == 7 ? modeLine_ >> 1 : modeLine_;
                const uint8_t d = m[((chb & 0xFE) << 8) + (ch & 0x3F) * 8 + l];
                pf = ((d >> (7 - pos % 8)) & 1) ? (ch >> 6) + 1 : 0;
                break;
            }
            case 8: case 0xA: case 0xD: case 0xE: {
                const int ppb = mode == 8 ? 16 : mode == 0xA ? 8 : 4;
                const uint8_t d = m[(lineMemscan_ + pos / ppb) & 0xFFFF];
                const int px = (pos % ppb) / (ppb / 4);
                pf = (d >> (6 - px * 2)) & 3;
                break;
            }
            case 9: case 0xB: case 0xC: {
                const int ppb = mode == 9 ? 16 : 8;
                const uint8_t d = m[(lineMemscan_ + pos / ppb) & 0xFFFF];
                pf = ((d >> (7 - (pos % ppb) / (ppb / 8))) & 1) ? 1 : 0;
                break;
            }
            case 0xF: {
                const uint8_t d = m[(lineMemscan_ + pos / 4) & 0xFFFF];
                hires = true;
                hiresBits = (d >> (6 - (pos % 4) * 2)) & 3;
                pf = 3;
                break;
            }
            default: break;
            }
        }
        // gracze i pociski
        int pbits = 0, mbits = 0;
        for (int i = 0; i < 4; ++i) {
            const int sz = g[0x08 + i] & 3;
            const int w = sz == 1 ? 2 : sz == 3 ? 4 : 1;
            const int off = cc - g[i];
            if (off >= 0 && off < 8 * w && (grafp[i] & (0x80 >> (off / w)))) pbits |= 1 << i;
            const int szm = (g[0x0C] >> (i * 2)) & 3;
            const int wm = szm == 1 ? 2 : szm == 3 ? 4 : 1;
            const int offm = cc - g[4 + i];
            if (offm >= 0 && offm < 2 * wm && (((grafm >> (i * 2)) & 3) & (2 >> (offm / wm)))) mbits |= 1 << i;
        }
        // kolizje
        const int pfc = hires ? 4 : pf ? (1 << (pf - 1)) : 0;
        for (int i = 0; i < 4; ++i) {
            if (mbits & (1 << i)) { coll_[i] |= pfc; coll_[8 + i] |= pbits; }
            if (pbits & (1 << i)) { coll_[4 + i] |= pfc; coll_[12 + i] |= pbits & ~(1 << i); }
        }
        // kolor
        const uint8_t prior = g[0x1B];
        const uint8_t hiresOn = static_cast<uint8_t>((g[0x18] & 0xF0) | (g[0x17] & 0x0E));
        uint8_t pfColor = hires ? (hiresBits ? hiresOn : g[0x18]) : pf == 0 ? g[0x1A] : g[0x15 + pf];
        uint8_t col = pfColor;
        int pm = pbits;
        const bool fifth = (prior & 0x10) != 0;
        if (!fifth) pm |= mbits;
        int pIdx = -1;
        uint8_t pcol = 0;
        if (pm) {
            const bool multi = (prior & 0x20) != 0;
            if (pm & 3) {
                pIdx = (pm & 1) ? 0 : 1;
                pcol = g[0x12 + pIdx];
                if (multi && (pm & 3) == 3) pcol = g[0x12] | g[0x13];
            } else {
                pIdx = (pm & 4) ? 2 : 3;
                pcol = g[0x12 + pIdx];
                if (multi && (pm & 12) == 12) pcol = g[0x14] | g[0x15];
            }
        }
        if (pIdx >= 0) {
            bool pfHigh = false;
            if (pf == 0) pfHigh = false;
            else if (prior & 1) pfHigh = false;
            else if (prior & 4) pfHigh = true;
            else if (prior & 2) pfHigh = pIdx >= 2;
            else if (prior & 8) pfHigh = pf == 1 || pf == 2 || hires;
            col = pfHigh ? pfColor : pcol;
        }
        const bool fifthOn = fifth && mbits;
        if (fifthOn && (pIdx < 0 || !(prior & 1))) col = g[0x19];
        const int x = (cc - 44) * 2;
        if (hires && pIdx < 0 && !fifthOn) {
            out[x] = pal[(hiresBits & 2) ? hiresOn : g[0x18]];
            out[x + 1] = pal[(hiresBits & 1) ? hiresOn : g[0x18]];
        } else {
            out[x] = out[x + 1] = pal[col];
        }
    }
}

// MARK: - Wątek gry

void Machine::startThread() {
    stop_ = false;
    logicTurn_ = false;
    cond_ = [] { return true; };
    parkedTag_ = 0;
    started_ = true;
    thread_ = std::thread([this] { threadMain(); });
}

void Machine::stopThread() {
    if (!started_) return;
    {
        std::unique_lock<std::mutex> lk(lock_);
        stop_ = true;
        logicTurn_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    started_ = false;
    stop_ = false;
    logicTurn_ = false;
}

void Machine::threadMain() {
    {
        std::unique_lock<std::mutex> lk(lock_);
        cv_.wait(lk, [&] { return logicTurn_; });
    }
    if (!stop_) {
        try {
            if (entry_) entry_(); else if (program) program();
        } catch (const MachineStop&) {
        }
    }
    std::unique_lock<std::mutex> lk(lock_);
    cond_ = [] { return false; };
    logicTurn_ = false;
    cv_.notify_all();
}

void Machine::runLogic() {
    if (!started_) {
        if (!program && !entry_) return;
        startThread();
    }
    while (cond_ && cond_()) {
        std::unique_lock<std::mutex> lk(lock_);
        logicTurn_ = true;
        cv_.notify_all();
        cv_.wait(lk, [&] { return !logicTurn_; });
    }
}

void Machine::waitUntil(const std::function<bool()>& cond) {
    if (cond()) return;
    std::unique_lock<std::mutex> lk(lock_);
    cond_ = cond;
    parkedTag_ = parkTag_;
    logicTurn_ = false;
    cv_.notify_all();
    cv_.wait(lk, [&] { return logicTurn_; });
    parkedTag_ = 0;
    if (stop_) throw MachineStop{};
}

void Machine::waitFrame() {
    const uint8_t t = m[0x14];
    waitUntil([this, t] { return m[0x14] != t; });
}

void Machine::waitVcount(int v) {
    waitUntil([this, v] { return vcount() == v; });
}

void Machine::restart(std::function<void()> entry) {
    stopThread();
    entry_ = std::move(entry);
}
