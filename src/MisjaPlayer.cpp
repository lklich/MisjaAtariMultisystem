//
//  MisjaPlayer.cpp
//  Misja (C++ / SDL3)
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/

//  Gracz ($7F6C-$8596): automat stanów ($CB, skok przez tablicę $859D), ruch, skoki, spadanie, drabiny, rzut granatem, 
//  sprawdzanie kafli pod i obok postaci, rysowanie postaci w pamięci graczy 0/1 ($BC00/$BD00).
//  $B9/$BA x/y   
//  $BB poprzednie y   
//  $CC kierunek ruchu (-1/0/1)   
//  $CD licznik skoku/spadania
//  $CE/$CF klatka animacji (baza/numer)   
//  $D4/$D5 blokada przycisku   
//  $D0-$D3 granat


#include "Misja.h"

// $7F6C (JSR $7F60)
void Misja::player7F6C() {
    playerState();
    player851F();
    playerDraw();
    M[0xBB] = M[0xBA];
    M[0x740C] = M[0x740D] = M[0xB9];
    const int a = fire();
    if (M[0xD4] != 0) {
        if (a != 0) M[0xD4] = 0;
        return;
    }
    if (a != 0) return;
    M[0xD5] = 0;
    shots8A54();
}

// $7F9A – wybór stanu z joysticka (gdy $CB < 4) i skok do procedury stanu
void Misja::playerState() {
    if (M[0xCB] < 4) {
        const int a = stick();
        for (int x = 0; !(M[0x85B9 + x] & 0x80); x += 2) {
            if (a == M[0x85B9 + x]) {
                M[0xCB] = M[0x85BA + x];
                break;
            }
        }
    }
    const int a = M[0xCB];
    if (a != 0) M[0x21F] = 2;
    const int x = (a << 1) & 0xFF;
    M[0x7FD2] = M[0x859D + x];
    M[0x7FD3] = M[0x859E + x];
    switch (M.word(0x7FD2)) {           // JMP $xxxx (samomodyfikujący się)
    case 0x7FD4: state0(); break;
    case 0x813C: state1(); break;
    case 0x81BE: state3(); break;
    case 0x8069: state4(); break;
    case 0x80C5: state5(); break;
    case 0x80FA: state6(); break;
    case 0x8040: state7(); break;
    case 0x8211: state8(); break;
    case 0x8268: state9(); break;
    case 0x827B: state10(); break;
    case 0x82B7: state11(); break;
    default: break;                      // $8410 (stany 12 i 13): RTS
    }
}

// $7FD4 – stoi
void Misja::state0() {
    if (onGround() == 0) {
        M[0xCB] = 7;
        return;
    }
    if (M[0x21F] == 0) {
        const Tile t = tileAt((M[0xB9] + 4) & 0xFF, (M[0xBA] + 0x1B) & 0xFF);
        if (t.a >= 2) {
            M[0xCB] = 0x0A;
            M[0x85C8] = M[0xCE];
            return;
        }
    }
    M[0xCD] = 0;
    M[0xCF] = 0;
    const int cc = M[0xCC];
    if (cc != 0) M[0xCE] = (cc & 0x80) ? 0 : 0x10;
    // zabezpieczenie: SERIN musi być równe ostatniej sumie kontrolnej z taśmy ($0484)
    if (!(M[0x862A] & 0x80) && (M[0x8629] & 0x80)) {
        for (int y = 3; y >= 0; --y) M[0x80 + y] = M[0x862B + y];
        const int a = read(M.word(0x80) + 0xFF) ^ read(M.word(0x82) + 0xFF);
        if (a != 0 && M[0x89] >= 0x22) {
            M[0x85A3] = 0x69;
            M[0x85A4] = 0x80;
        }
    }
}

// $8040 – spadanie
void Misja::state7() {
    if (!(M[0xCD] & 0x80)) M[0xCD]++;
    if (!fall()) {
        M[0xCF] = 1;
        return;
    }
    if (M[0xCD] != 0) sound(2);
    M[0xCB] = 0;
    const int a = M[0xCD] - 0x13;
    if (a < 0) {
        M[0xCF] = 1;
        return;
    }
    damage((a << 2) & 0xFF);
}

// $8069 – skok w górę
void Misja::state4() {
    if (stick() == 1 && topOfLadder() && (M[0x8E] & 0x80)) {
        M[0xCB] = 0x0C;
        M[0xCF] = 0;
        return;
    }
    if (onGround() != 0) {
        M[0x85] = (M[0xB9] + 1) & 0xFF;
        if ((M[0x85] & 7) < 3) {
            const Tile t = tileAt((M[0xB9] + 1) & 0xFF, (M[0xBA] + 8) & 0xFF);
            if (t.a != 0 && t.a < 3) {
                M[0xBA]--;
                M[0xB9] = M[0x85] & 0xF8;
                M[0xCB] = 8;
                return;
            }
        }
    }
    M[0xCB] = 5;
    readDirection();
    M[0xCD] = 0x0C;
    M[0xCF] = 0;
    M[0xBA]--;
    state5();
}

// $80C5 – lot w górę
void Misja::state5() {
    if (headroom() == 0 && M[0xCD] != 0) {
        const int x = M[0xCD];
        const int a = M[0x8653 + x];
        if (x < 0x0A) M[0xCF] = a;
        if (walk()) M[0xCC] = 0;
        if (!jumpUp()) {
            M[0xCD]--;
            return;
        }
    }
    M[0xCD] = 0;
    if (ladder() == 0) {
        M[0xCB] = 8;
        return;
    }
    M[0xCB] = 6;
    state6();
}

// $80FA – opadanie po skoku
void Misja::state6() {
    const int x = (M[0xCD] - 1) & 0xFF;
    if (x < 4) M[0xCF] = M[0x865E + x];
    if (onGround() == 0 && !walk() && !fall()) {
        if (!(M[0xCD] & 0x80)) M[0xCD]++;
        return;
    }
    M[0xCB] = 7;
    M[0xCF] = 0;
    // $8124: LDA #$FF / BPL nie skacze – pułapka zabezpieczenia ($8630 ≠ 0 psuje kafle)
    {
        const int x = M.random() & 0x3F;
        if (M[0x8630] != 0) M[0x1200 + x] = M.random();
    }
    state7();
}

// $813C – chodzenie
void Misja::state1() {
    M[0xCD] = 0;
    readDirection();
    walk();
    if (onGround() == 0) M[0xCB] = 7;
    if (!(M[0xD6] & 1)) return;
    M[0xCF]++;
    if ((M[0xCF] & 3) == 0) { sound(2); return; }
    // $8162: pułapka zabezpieczenia ($862F ≠ 0 psuje adres ekranu)
    if (M[0x862F] != 0 && M[0xB9] == M[0xD6]) {
        M[0x95] = 0xD0;
        M[0x862F] = 0;
    }
}

// $817C – rzut granatem (fire w przysiadzie)
void Misja::throwGrenade() {
    M[0xD4] = 0xFF;
    if (M[0xE8] == 0) return;
    if (M[0xCE] >= 0x20) return;
    if (M[0xD0] != 0) return;
    const int a = fire();
    if (M[0xD5] != 0) {
        if (a != 0) M[0xD5] = 0;
        return;
    }
    if (a != 0) return;
    M[0xD5] = 0xFF;
    M[0x85C8] = M[0xCE];
    M[0xCE] = M[0xCE] == 0x10 ? 0x50 : 0x40;
    M[0xCF] = 0;
    M[0xCB] = 0x0B;
    M[0xE8]--;
}

// $81BE – przysiad (joystick w dół)
void Misja::state3() {
    if (onGround() == 0) {
        M[0xCB] = 7;
        return;
    }
    M[0xCF] = 0;
    if (fire() == 0) { throwGrenade(); return; }
    if (topOfLadder() && (M[0x8E] & 1)) {
        M[0xCB] = 0x0D;
        return;
    }
    M[0x85] = (M[0xB9] + 1) & 0xFF;
    if ((M[0x85] & 7) >= 3) { throwGrenade(); return; }
    const Tile t = tileAt((M[0xB9] + 1) & 0xFF, (M[0xBA] + 0x1B) & 0xFF);
    if (t.a != 2) { throwGrenade(); return; }
    M[0xBA]++;
    M[0xB9] = M[0x85] & 0xF8;
    M[0xCB] = 8;
    M[0xCE] = 0x20;
}

// $8211 – na drabinie
void Misja::state8() {
    M[0xCE] = 0x20;
    M[0xCC] = 0;
    M[0xCD] = 0;
    auto leave = [this] {
        M[0xCB] = 9;
        // zabezpieczenie: kierunek portu A (PIA) ustawiony przez program startowy
        M.pia(2, 0x38);
        if (!(M.piaRead(0) & 0x80)) M[0x8630]--;
        M.pia(2, 0x3C);
    };
    if (ladder() != 0) { leave(); return; }
    const int a = stick();
    if (a == 2) {
        M[0xBA]++;
        if (ladder() != 0) { leave(); return; }
        M[0xBA]++;
        M[0xCF] = (M[0xCF] + 1) & 0xFF;
    } else if (a == 1) {
        M[0xBA]--;
        if (ladder() != 0) { leave(); return; }
        M[0xBA]--;
        M[0xCF] = (M[0xCF] + 0xFF) & 0xFF;
    } else if (a != 0) {
        leave();
    }
}

// $8268 – zejście z drabiny
void Misja::state9() {
    state7();
    M[0xCB] = 9;
    if (stick() != 1) M[0xCB] = 0;
}

// $827B – bezczynność (rozglądanie się)
void Misja::state10() {
    if (stick() != 0) {
        M[0xCB] = 0;
        M[0xCE] = M[0x85C8];
        M[0xCF] = 0;
        return;
    }
    M[0xCE] = 0x30;
    if (M[0xD6] & 7) return;
    if (--M[0x8631] & 0x80) {
        M[0x8631] = 3;
        M[0x8632] = M.random();
    }
    const int x = (M[0x8632] & 0x1C) | M[0x8631];
    M[0xCF] = M[0x8633 + x];
}

// $82B7 – rzut granatem (animacja i wyrzucenie)
void Misja::state11() {
    if (M[0xD6] & 1) return;
    const int a = M[0xCF];
    M[0xCF]++;
    if (a == 4) {
        M[0xC8] = (M[0xB9] + 3) & 0xFF;
        M[0xC9] = (M[0xBA] + 0x10) & 0xFF;
        M[0xD0] = 1;
        M[0xD1] = 8;
        M[0xD2] = 0;
        M[0xD3] = M[0xCE] == 0x50 ? 1 : 0xFF;
    }
    if (M[0xCF] < 8) return;
    M[0xCE] = M[0x85C8];
    M[0xCB] = 0;
    M[0xCF] = 0;
    if (M[0x82FF] == 0x60) return;       // RTS zamieniony na NOP przez $84C3
    // $8300 – śmieci ze stosu (na Atari zależne od wcześniejszych wywołań)
    M[0x81] = 1;
    M[0x80] = 0xBF;
    if (M[0x1C2] | M[0x1C5]) M[0x862F]++;
}

// $8317 – stoi na czymś? (wartość ≠ 0 = tak)
int Misja::onGround() {
    M[0x85] = (M[0xBA] + 0x1B) & 0xFF;
    if (M[0x85] & 0x0F) return 0;
    Tile t = tileAt((M[0xB9] + 2) & 0xFF, M[0x85]);
    if (t.a >= 2) return t.a;
    t = tileAt((M[0xB9] + 5) & 0xFF, M[0x85]);
    return t.a >= 2 ? t.a : 0;
}

// $834A – ściana nad głową / obok (≠ 0 = przeszkoda)
int Misja::headroom() {
    Tile t = tileAt((M[0xB9] + 2) & 0xFF, (M[0xBA] + 1) & 0xFF);
    if (t.a >= 4) return t.a;
    t = tileAt((M[0xB9] + 5) & 0xFF, (M[0xBA] - 1) & 0xFF);
    return t.a >= 4 ? t.a : 0;
}

// $8376 – ściana w kolumnie X (trzy punkty wysokości postaci)
int Misja::wallAt(int x) {
    M[0x85] = x;
    Tile t = tileAt(x, (M[0xBA] + 1) & 0xFF);
    if (t.a >= 4) return t.a;
    t = tileAt(M[0x85], (M[0xBA] + 0x0F) & 0xFF);
    if (t.a >= 4) return t.a;
    t = tileAt(M[0x85], (M[0xBA] + 0x1A) & 0xFF);
    if (t.a >= 4) return t.a;
    return 0;
}

// $83B2 – drabina (0 = można stać/wchodzić)
int Misja::ladder() {
    if (M[0xB9] & 7) return M[0xB9] & 7;
    if (int a = headroom()) return a;
    Tile t = tileAt(M[0xB9], (M[0xBA] + 0x0C) & 0xFF);
    M[0x85] = (t.a == 1 || t.a == 2) ? 1 : 0;
    t = tileAt(M[0xB9], (M[0xBA] + 0x1B) & 0xFF);
    const int x = t.a != 0 ? 1 : 0;
    if (t.a >= 3) return 1;
    return (x | M[0x85]) ^ 1;
}

// $83F4 – nad wejściem do szybu ($8D = $FC i dwa kafle $2A)
bool Misja::topOfLadder() {
    if (M[0x8D] != 0xFC) return false;
    const Tile t = tileAt(M[0xB9], (M[0xBA] + 0x1B) & 0xFF);
    if (t.x != 0x2A) return false;
    return (M[0xBA01 + t.y] & 0x3F) == 0x2A;
}

// $8412 (JSR $7F66) – kafel pod punktem (X, Y): a = właściwość ($1200, bity 0-3), x = kod, y = indeks
Misja::Tile Misja::tileAt(int px, int py) {
    int a = py - 0x20;
    if (a < 0) a = 0xF0;
    M[0x8411] = a & 0xF0;
    int y = ((px >> 3) + M[0x8411]) & 0xFF;
    auto prop = [&](int code) {
        Tile t;
        t.x = code & 0x3F;
        t.a = M[0x1200 + t.x] & 0x0F;
        t.y = y;
        return t;
    };
    if (!(y & 0x80)) return prop(M[0xBA00 + y]);
    if (y >= 0xB0) {
        y = (y + 0x10) & 0xFF;
        return prop(M[0xBA00 + y]);
    }
    y = (y - 0x10) & 0xFF;
    Tile t = prop(M[0xBA00 + y]);
    if (t.a == 2) t.a = 1;
    return t;
}

// $8459 – spadanie o krok ($CD); true (C) = wylądował
bool Misja::fall() {
    int y = jumpStep();
    for (;;) {
        if (ladder() == 0) {
            // przy drabinie: zatrzymanie na niej albo na podłodze
            M[0xBA]--;
            const int g = onGround();
            M[0xBA]++;
            if (g == 0) {
                const int py = (M[0xBA] + 0x1B) & 0xFF;
                if (!(py & 0x0F) && tileAt(M[0xB9], py).a == 2) {
                    M[0xCB] = 0;
                    return false;
                }
            }
            M[0xCB] = 8;
            return false;
        }
        if (onGround() != 0) return true;
        if (y == 0) return false;
        M[0xBA]++;
        y--;
        if (y == 0) return false;
    }
}

// $84A1 – skok w górę o krok ($CD); true (C) = uderzył w sufit
bool Misja::jumpUp() {
    int y = jumpStep();
    for (;;) {
        if (ladder() == 0) {
            M[0xCB] = 8;
            return false;
        }
        if (headroom() != 0) return true;
        if (y == 0) break;
        M[0xBA]--;
        y--;
        if (y == 0) break;
    }
    M[0x82FF] = 0xEA;
    return false;
}

// $84CA – krok w poziomie ($CC); true (C) = ściana
bool Misja::walk() {
    const int cc = M[0xCC];
    if (cc == 0) return false;
    if (!(cc & 0x80)) {
        M[0xCE] = 0x10;
        if (wallAt((M[0xB9] + 6) & 0xFF) != 0) return true;
        M[0xB9]++;
        return false;
    }
    M[0xCE] = 0;
    if (wallAt((M[0xB9] + 1) & 0xFF) != 0) return true;
    M[0xB9]--;
    return false;
}

// $84F0 – kierunek z joysticka do $CC
void Misja::readDirection() {
    const int a = stick();
    int x = 0;
    if (a & 4) x = 0xFF;
    if (a & 8) x = 1;
    M[0xCC] = x;
}

// $8504 – liczba kroków dla licznika $CD
int Misja::jumpStep() {
    const int x = M[0xCD];
    if (x < 0x0D) return M[0x8512 + x];
    return x >> 1;
}

// $851F (JSR $7F63) – wymazanie postaci (35 linii od $BB - 8)
void Misja::player851F() {
    int y = (M[0xBB] - 8) & 0xFF;
    for (int x = 0x23; x != 0; --x) {
        M[0xBC00 + y] = 0;
        M[0xBD00 + y] = 0;
        y = (y + 1) & 0xFF;
    }
}

// $8534 – rysowanie postaci: dwie klatki 16-liniowe (góra i dół) z tablicy $85C9
void Misja::playerDraw() {
    int x = (M[0xBA] - 5) & 0xFF;
    const int y = (((M[0xCF] & 7) << 1) + M[0xCE]) & 0xFF;
    const int top = M[0x85C9 + y];
    x = drawFrame(M[0x85CA + y], x);
    drawFrame(top, x);
}

// $854E – 16 linii klatki A do graczy 0 i 1 od linii X; zwraca X + 16
int Misja::drawFrame(int a, int x) {
    framePtr(a);
    const int p0 = M.word(0x80), p1 = M.word(0x82);
    for (int y = 0; y < 0x10; ++y) {
        M[0xBC00 + x] = M[p0 + y];
        M[0xBD00 + x] = M[p1 + y];
        x = (x + 1) & 0xFF;
    }
    return x;
}

// $8564 (JSR $7F69) – adresy klatki: $80 = $6000 + (A & $7F) × 16, $82 = $80 + $400
void Misja::framePtr(int a) {
    const int base = 0x6000 + ((a & 0x7F) << 4);
    M.setWord(0x80, base);
    M.setWord(0x82, base + 0x400);
}

// $8583 – joystick (bity: 1 góra, 2 dół, 4 lewo, 8 prawo)
int Misja::stick() {
    const int a = M.piaRead(0);
    M[0x84] = a;
    return ((a >> 4) & a) ^ 0x0F;
}

// $8596 – przycisk (0 = wciśnięty)
int Misja::fire() { return M.trig(0) & M.trig(1); }
