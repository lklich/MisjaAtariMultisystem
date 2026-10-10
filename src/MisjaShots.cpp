//
//  MisjaShots.cpp
//  Misja (C++ / SDL3)
//
//  Granat, pociski gracza i odłamki ($86F0-$8BEF).
//
//  Granat: $C8/$C9 x/y, $CA poprzednie y, $D0 faza (0 brak, 1 w górę, 2 w dół, 3 wybuch),
//          $D1 prędkość, $D2 klatka wybuchu, $D3 kierunek (gracze 2 i 3, $BE00/$BF00).
//  Pociski: $BC-$BF x, $C0-$C3 y, $C4-$C7 krok (0 = brak) – pociski w pamięci $BB00.
//  Odłamki (8): $8892 x, $889A y, $88A2 rodzaj (0 = brak), $88AA czas lotu;
//          rysowane jako znaki $D8-$DF w ekranie.
//
//  Przeniesienie (C) po $88B2 trafia do głównej pętli (ADC w pokazie), dlatego te
//  procedury ustawiają c_ tak jak procesor.
//

#include "Misja.h"

// $86FC – ruch i rysowanie odłamków
void Misja::debris() {
    M[0x88] = 7;
    do {
        const int x = M[0x88];
        if (M[0x88A2 + x] != 0) {
            debrisMove();
            if (debrisCheck() != 0) debrisDraw();
        }
        M[0x88]--;
    } while (!(M[0x88] & 0x80));
}

// $8717 – odłamek w ścianie / poza ekranem? (0 = zniknął)
int Misja::debrisCheck() {
    const int x = M[0x88];
    const int a = M[0x8892 + x];
    if (a & 0x80) {
        M[0x88A2 + x] = 0;
        return 0;
    }
    M[0x84] = a >> 3;
    const int s = M[0x889A + x] << 1;
    c_ = (s & 0x100) != 0;
    if (s & 0x80) {
        M[0x88A2 + x] = 0;
        return 0;
    }
    const int y = ((s & 0x70) + M[0x84]) & 0xFF;
    const int code = M[0xBA00 + y] & 0x7F;
    const int p = M[0x1200 + code] & 0x0F;
    c_ = p >= 3;
    if (c_) {
        M[0x88A2 + x] = 0;
        return 0;
    }
    return 1;
}

// $8747 – ruch odłamka
void Misja::debrisMove() {
    const int x = M[0x88];
    c_ = false;
    int y = M[0x88A2 + x];
    if (y == 0) return;
    auto adc = [&](int addr, int v) {
        const int r = M[addr] + v + (c_ ? 1 : 0);
        c_ = r > 0xFF;
        M[addr] = r & 0xFF;
    };
    if (y != 1 && y != 7) {
        adc(0x8892 + x, M[0x886B + y]);
        c_ = false;
        adc(0x889A + x, M[0x8870 + y]);
        return;
    }
    // rodzaje 1 i 7: lot po łuku (tablica $8877)
    y = (y + 1) & 0xFF;
    if (y != 2) y = 0xFE;
    c_ = false;
    adc(0x8892 + x, y);
    y = M[0x88AA + x];
    M[0x88AA + x]++;
    c_ = y >= 0x1A;
    if (c_) y = 0x1A;
    adc(0x889A + x, M[0x8877 + y]);
}

// $8799 – rysowanie odłamka: znak $D8 + nr na tle znaku spod spodu
void Misja::debrisDraw() {
    int x = M[0x88];
    M[0x82] = (M[0x889A + x] << 2) & 0xE0;
    M[0x82] = ((M[0x8892 + x] >> 2) + M[0x82]) & 0xFF;
    int y = M[0x82];
    charGraphics(M[M.word(0x58) + y] & 0x7F);
    M[0x80] = ((M[0x88] + 0x18) << 3) & 0xFF;
    M[0x81] = 0x52;
    for (int k = 1; k >= 0; --k) {
        for (y = 7; y >= 0; --y) M[M.word(0x80) + y] = M[M.word(0x86) + y];
        M[0x81] = (M[0x81] + 4) & 0xFF;
        M[0x87] = (M[0x87] + 4) & 0xFF;
    }
    x = M[0x88];
    M[0x81] = (M[0x889A + x] & 7) >= 4 ? 0x56 : 0x52;
    y = (M[0x889A + x] & 3) << 1;
    const int k = M[0x8892 + x] & 3;
    const int mask = M[0x8865 + k];
    const int p = M.word(0x80);
    M[p + y] = (mask & M[p + y]) | M[0x8869 + k];
    y++;
    M[p + y] = (mask & M[p + y]) | M[0x8869 + k];
    y = M[0x82];
    M[M.word(0x58) + y] = (M[0x88] + 0xD8) & 0xFF;
    c_ = false;
}

// $881C (JSR $86F9) – nowy odłamek: A rodzaj, X x, Y y
void Misja::spawnDebris(int a, int x, int y) {
    M[0x85] = a;
    M[0x84] = x;
    int k = 0;
    for (; k < 8; ++k) {
        if (M[0x88A2 + k] == 0) break;
    }
    if (k >= 8) return;
    M[0x8892 + k] = M[0x84];
    M[0x889A + k] = y;
    M[0x88A2 + k] = M[0x85];
    M[0x88AA + k] = 0;
    M[0x88] = k;
    if (debrisCheck() == 0) return;
    sound(9);
    // $884B: LDA #$FF / BPL nie skacze – pułapka zabezpieczenia ($8A52 ≠ 0 psuje adres ekranu)
    if ((M.random() & 0x1F) != 0) return;
    if (M[0x8A52] == 0) return;
    M[0x59] = 0xAD;
}

// $88B2 (JSR $86F0) – granat, pociski, odłamki; zwraca C
bool Misja::enemies88B2() {
    grenade();
    grenadeErase();
    grenadeDraw();
    M[0xCA] = M[0xC9];
    const int x = (M[0xC8] - 3) & 0xFF;
    M[0x740E] = M[0x740F] = x;
    bulletsErase();
    bulletsMove();
    bulletsDraw();
    debris();
    return c_;
}

// $88D6 – lot i wybuch granatu
void Misja::grenade() {
    if (M[0xC8] < 2 || M[0xC8] >= 0x7D || M[0xC9] >= 0xA0) {
        M[0xD0] = 0;
        return;
    }
    switch (M[0xD0]) {
    case 1:
        M[0x85] = 0xFF;
        if (grenadeStep()) return;
        if (--M[0xD1] == 0) M[0xD0] = 2;
        return;
    case 2:
        M[0x85] = 1;
        if (!grenadeStep()) M[0xD1]++;
        return;
    default:
        if (M[0xD6] & 1) return;
        if (M[0xD2] == 0) {
            explosion();
            sound(1);
        }
        M[0xD2]++;
        if (M[0xD2] < 6) return;
        M[0xD0] = 0;
        M[0xC8] = 0xBF;
        return;
    }
}

// $893A – krok granatu; C = uderzył (wybuch)
bool Misja::grenadeStep() {
    int y = (M[0xD1] >> 1) + 1;
    do {
        M[0xC9] = (M[0xC9] + M[0x85]) & 0xFF;
        const int p = tileAt(M[0xC8], M[0xC9]).a;
        if ((M[0xC9] & 0x0F) == 0) {
            M[0x84] = (((M[0xD0] - 1) << 1) ^ 2) & 0xFF;
            const int a = (p - M[0x84]) & 0xFF;
            if (!(a & 0x80) && a >= 2) {
                M[0xD0] = 3;
                return true;
            }
        }
        y = (y - 1) & 0xFF;
    } while (y != 0);
    M[0xC8] = (M[0xC8] + M[0xD3] + M[0xD3]) & 0xFF;
    if (tileAt(M[0xC8], M[0xC9]).a >= 4) {
        M[0xD0] = 3;
        return true;
    }
    return false;
}

// $898D – wymazanie granatu
void Misja::grenadeErase() {
    int x = (M[0xCA] - 8) & 0xFF;
    for (int y = 0x0F; y >= 0; --y) {
        M[0xBE00 + x] = 0;
        M[0xBF00 + x] = 0;
        x = (x + 1) & 0xFF;
    }
}

// $89A2 – rysowanie granatu (klatka $8B08 + $D2)
void Misja::grenadeDraw() {
    if (M[0xD0] == 0) return;
    int x = (M[0xC9] - 8) & 0xFF;
    framePtr(M[0x8B08 + M[0xD2]]);
    const int p0 = M.word(0x80), p1 = M.word(0x82);
    for (int y = 0; y < 0x10; ++y) {
        M[0xBE00 + x] = M[p0 + y];
        M[0xBF00 + x] = M[p1 + y];
        x = (x + 1) & 0xFF;
    }
}

// $89C8 – wybuch niszczy kafle po obu stronach
void Misja::explosion() {
    explodeTile((M[0xC8] - 3) & 0xFF, (M[0xC9] - 1) & 0xFF);
    explodeTile((M[0xC8] + 3) & 0xFF, (M[0xC9] - 1) & 0xFF);
}

// $89DB
void Misja::explodeTile(int px, int py) {
    const Tile t = tileAt(px, py);
    const int a = M[0x1200 + t.x] & 0xF0;
    int x = M[0xBA00 + t.y];
    if (a == 0x10) x = 0x27;
    if (a == 0x90 || a == 0xA0 || a == 0x50) x = 0x26;
    if (x != M[0xBA00 + t.y]) addScore(0x25);
    M[0xBA00 + t.y] = x;
    M[0x92] = 0;
    M[0x93] = 0;
}

// $8A10 – ruch pocisków
void Misja::bulletsMove() {
    for (M[0x85] = 3; !(M[0x85] & 0x80); M[0x85]--) {
        int x = M[0x85];
        if (M[0xC4 + x] == 0) continue;
        int a = M[0xBC + x];
        if (a < 4 || a >= 0x7D) {
            M[0xBC + x] = 0xBF;
            M[0xC4 + x] = 0;
            a = 0;
        }
        const int y = M[0xC0 + x];
        if (a != 0) a = tileAt(a, y).a;
        c_ = a >= 4;
        if (c_) bulletKill();
        x = M[0x85];
        M[0xBC + x] = (M[0xC4 + x] + M[0xBC + x] + (c_ ? 1 : 0)) & 0xFF;
    }
    for (int x = 3; x >= 0; --x) M[0x7414 + x] = M[0xBC + x];
}

// $8A54 (JSR $86F6) – strzał
void Misja::shots8A54() {
    M[0x21F] = 2;
    M[0x81] = 2;
    if (M[0xD6] & 0x40) {
        // zabezpieczenie: DFLAGS ($0240) musi być równe $9B
        M[0x80] = 0x40;
        if (M[0x0240] != M[0x8A53]) M[0x8A52]--;
    }
    if (M[0xE7] == 0) return;
    if (M[0xCB] >= 0x0C) return;
    M[0xD4] = 3;
    int x = 3;
    for (; x >= 0; --x) {
        if (M[0xC4 + x] == 0) break;
    }
    if (x < 0) return;
    const int y = M[0xCE];
    if (y >= 0x20) return;
    int a = M[0xB9];
    if (y != 0) a = (a + 7) & 0xFF;
    M[0xBC + x] = a;
    a = (M[0xBA] + 0x0C) & 0xFF;
    if (a < 0x20) return;
    M[0xC0 + x] = a;
    M[0xC4 + x] = y != 0 ? 4 : 0xFC;
    M[0xE7]--;
    sound(8);
}

// $8AB4
void Misja::bulletKill() {
    const int x = M[0x85];
    M[0xC4 + x] = 0;
    M[0xBC + x] = 0xBF;
}

// $8ABF
void Misja::bulletsErase() {
    for (int x = 3; x >= 0; --x) M[0xBB00 + M[0xC0 + x]] = 0;
}

// $8ACC
void Misja::bulletsDraw() {
    M[0x84] = 0xC0;
    for (int x = 3; x >= 0; --x) {
        if (M[0xC4 + x] != 0) {
            const int y = M[0xC0 + x];
            M[0xBB00 + y] |= M[0x84];
        }
        M[0x84] >>= 2;
    }
    c_ = true;   // ostatnie LSR $84 wysuwa jedynkę
}

// $8AE8 (JSR $86F3) – usunięcie granatu, pocisków i odłamków
void Misja::enemies8AE8() {
    grenadeErase();
    M[0xD0] = 0;
    for (int x = 3; x >= 0; --x) {
        M[0xC4 + x] = 0;
        M[0x7414 + x] = 0xBF;
    }
    for (int x = 7; x >= 0; --x) M[0x88A2 + x] = 0;
}
