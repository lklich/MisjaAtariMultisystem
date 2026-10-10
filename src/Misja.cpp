//
//  Misja.cpp
//  Misja (C++ / SDL3)
//

#include "Misja.h"

Misja::Misja(Machine& machine, const std::vector<uint8_t>& image) : M(machine) {
    loadImage(M, image);
    M.program = [this] { start(); };
    M.dli = [this] { dli(); };
    M.deferredVBI = [this] { deferredVBI(); };
}

void Misja::loadImage(Machine& m, const std::vector<uint8_t>& image) {
    for (int a = 0x0480; a < 0x0600; ++a) m[a] = image[a];
    for (int a = 0x1200; a < 0xA000; ++a) m[a] = image[a];
    // Program startowy ($0486): kopia bufora kasety, strona zerowa, rejestry PIA, blok SIO.
    for (int y = 0x7F; y >= 0; --y) m[0x0400 + y] = m[0x0580 + y];   // $0400 = ostatni rekord
    for (int y = 0x14; y >= 0; --y) m[0x80 + y] = m[0x05CD + y];
    int sum = 0x55;                                                    // $01: suma kontrolna
    for (int a = 0x1200; a < 0xA000; ++a) sum ^= m[a];
    m[0x01] = sum;
    m.setSerin(sum);                                                   // ostatni bajt z taśmy
    m[0x0240] = m[0x0480];                                             // DFLAGS
    m[0x0303] = 0x40;                                                  // DSTATS
    m[0x0304] = 0x00;                                                  // DBUF
    m[0x0305] = 0x06;
    m[0x0306] = 0x06;                                                  // DTIMLO
    m[0x0308] = 0x82;                                                  // DBYT
    m[0x02C6] = 0x00;                                                  // COLOR2
    m.pia(2, 0x30);                                                    // PACTL: dostęp do DDRA
    m.pia(0, 0xFF);                                                    // DDRA = $FF
    m.pia(2, 0x3C);
    m.pia(0, 0xFF);
}

int Misja::read(int addr) {
    addr &= 0xFFFF;
    if (addr >= 0xD000 && addr < 0xD800) {
        switch (addr & 0xFF00) {
        case 0xD000: {
            const int r = addr & 0x1F;
            if (r < 0x10) return M.collision(r);
            if (r < 0x14) return M.trig(r - 0x10);
            if (r == 0x1F) return M.consol();
            return 0x0F;
        }
        case 0xD200:
            if ((addr & 0x0F) == 0x0A) return M.random();
            if ((addr & 0x0F) == 0x09) return M.kbcode();
            if ((addr & 0x0F) == 0x0D) return M.serin();
            return 0xFF;
        case 0xD300: return M.piaRead(addr & 3);
        case 0xD400: return (addr & 0x0F) == 0x0B ? M.vcount() : 0xFF;
        default: return 0xFF;
        }
    }
    return M[addr];
}

void Misja::dli() {
    switch (M.word(0x200)) {
    case 0x6BB6: titleDli(); break;
    case 0x7D99: gameDli(); break;
    default: break;
    }
}

void Misja::deferredVBI() {
    if (M.word(0x224) == 0x70BC) soundVBI();
}
