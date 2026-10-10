//
//  MisjaSound.cpp
//  Misja (C++ / SDL3)
//
//  Dźwięk ($7000-$7152): przydział efektów do 4 kanałów POKEY i odtwarzanie ich
//  w przerwaniu VBI (tablice głośności i częstotliwości $7173/$7273).
//
//  $7157+X  numer efektu (bity 4-7) i głośność / krok (bity 0-3)
//  $715B+X  licznik ramek do następnego kroku
//  $715F+X  długość kroku (bity 0-3), bit 7 = efekt zapętlony
//

#include "Misja.h"

// $7000 – A < $80: efekt nr (A & $0F), bit 4 = zapętlony; A >= $80: cisza i instalacja VBI
void Misja::sound(int a) {
    if (a & 0x80) {
        for (int x = 7; x >= 0; --x) {
            M[0x7157 + x] = 0;
            M.pokey(x, 0);
            M[0x715B + x] = 0;
        }
        M.pokey(8, 0);
        M.pokey(0x0F, 3);
        M.setWord(0x224, 0x70BC);          // SETVBV 7, $70BC
        return;
    }
    const bool loop = (a & 0x10) != 0;
    M[0x7155] = (a << 4) & 0xFF;
    if (!loop) {
        soundStart();
        return;
    }
    for (int x = 3; x >= 0; --x) {
        if ((M[0x7157 + x] & 0xF0) == M[0x7155] && (M[0x715F + x] & 0x80)) return;
    }
    const int x = soundStart();
    M[(0x715F + x) & 0xFFFF] |= 0x80;
}

// $704C – zwraca X (kanał albo $FF, gdy żaden nie był wolny)
int Misja::soundStart() {
    for (int x = 3; x >= 0; --x) {
        if ((M[0x7157 + x] & 0xF0) == M[0x7155] && (M[0x715F + x] & 0x80)) {
            // ten sam efekt zapętlony – wyłączenie
            M[0x715B + x] = 1;
            M[0x715F + x] = 0;
            M[0x7157 + x] = 0;
            return x;
        }
    }
    int x = 3;
    for (; x >= 0; --x) {
        if (!(M[0x715F + x] & 0x80) && M[0x7157 + x] == 0) break;
    }
    if (x < 0) {
        // kanał z najcichszym efektem
        int y = 0x0F;
        M[0x7153] = 0x0F;
        for (x = 3; x >= 0; --x) {
            if (M[0x715F + x] & 0x80) continue;
            const int v = M[0x7157 + x] & 0x0F;
            if (v < M[0x7153]) {
                M[0x7153] = v;
                y = x;
            }
        }
        if (y >= 4) return 0xFF;
        x = y;
    }
    const int y = M[0x7155] >> 4;
    const int a = M[0x7163 + y];
    M[0x715B + x] = a;
    M[0x715F + x] = a;
    M[0x7157 + x] = M[0x7155] | 0x0F;
    return x;
}

// $70BC – VBI: kolejny krok każdego kanału
void Misja::soundVBI() {
    for (int x = 3; x >= 0; --x) {
        bool silence = false;
        if (M[0x715B + x] == 0) {
            silence = true;
        } else if (soundStep(x)) {
            silence = true;
        } else {
            if (--M[0x715B + x] != 0) continue;
            M[0x715B + x] = M[0x715F + x] & 0x0F;
            if (M[0x7157 + x] & 0x0F) {
                M[0x7157 + x]--;
                continue;
            }
            if (!(M[0x715F + x] & 0x80)) {
                M[0x7157 + x] = 0;
                M[0x715B + x] = 0;
                M[0x715F + x] = 0;
                continue;
            }
            // zapętlony – od początku
            M[0x7157 + x] |= 0x0F;
            M[0x715B + x] = M[0x715F + x] & 0x0F;
            if (M[0x715B + x] != 0) continue;
            silence = true;
        }
        // $7109
        while (silence) {
            if (M[0x715F + x] & 0x80) {
                M[0x7157 + x] |= 0x0F;
                M[0x715B + x] = M[0x715F + x] & 0x0F;
                if (M[0x715B + x] != 0) break;
                continue;   // (na Atari ta sama pętla – w danych gry nie występuje)
            }
            M.pokey(x * 2, 0);
            M.pokey(x * 2 + 1, 0);
            M[0x7157 + x] = 0;
            M[0x715B + x] = 0;
            M[0x715F + x] = 0;
            break;
        }
    }
}

// $711B – ustawienie AUDC/AUDF kanału X dla bieżącego kroku; C = koniec efektu ($FF)
bool Misja::soundStep(int x) {
    const bool second = (M[0x7157 + x] & 0x80) != 0;
    M[0x7156] = second ? 0x80 : 0;
    const int y = (M[0x7157 + x] << 1) & 0xFF;
    const int base = second ? 0x7273 : 0x7173;
    const int c = M[base + y];
    if (c == 0xFF) return true;
    M.pokey(x * 2 + 1, c);
    M.pokey(x * 2, M[base + y + 1]);
    return false;
}
