//
//  MisjaRooms.cpp
//  Misja (C++ / SDL3)
//
//  Elementy specjalne pomieszczeń ($9690-$9FFF): windy i szyby ($8D – rodzaj z tablicy
//  $B100, $8F/$90/$91 – stan i położenie), przełączniki ($9E6C), migające ściany,
//  zapadnie, przedmioty do zebrania, miejsca na pojemniki z uranem ($9C9E), zapamiętany
//  stan zniszczeń w pomieszczeniach ($B000, $9D81). Rodzaj kafla to bity 4-7 tablicy $1200.
//

#include "Misja.h"

// $969F (JSR $9690) – co ramkę
void Misja::rooms969F() {
    tileAnimations();
    liftChars();
    switch (M.word(0x96A6)) {            // JSR $xxxx (samomodyfikujący się)
    case 0x96C6: lift96C6(); break;
    case 0x9771: switch9771(); break;
    case 0x978A: switch978A(); break;
    default: break;                      // $96B1: RTS
    }
    if (!(M[0xD6] & 1)) return;
    M[0x92]++;
    M[0x93]++;
}

// $96B2 (JSR $9699) – po wejściu do pomieszczenia
void Misja::rooms96B2() {
    roomSwitchesInit();
    M[0x8D] = M[0xB100 + M[0x89]];
    if (M[0x8D] == 0x20) return;
    specialSetup();
    switch (M.word(0x96C4)) {            // JMP $xxxx (samomodyfikujący się)
    case 0x9925: lift9925(); break;
    case 0x996A: door996A(); break;
    case 0x9995: {                       // jeden krok $9999
        int x = M[0x90];
        shaftStep(0, x, 0x10);
        break;
    }
    default: break;
    }
}

// $96C6 – jazda windą (stan gracza $0C w górę, $0D w dół)
void Misja::lift96C6() {
    int step;
    if (M[0xCB] == 0x0C) {
        M[0x8F]++;
        if (!(M[0x8F] & 1)) { liftEnd(); return; }
        step = 0xF0;
    } else if (M[0xCB] == 0x0D) {
        M[0x8F]++;
        if (M[0x8F] & 1) { liftEnd(); return; }
        step = 0x10;
    } else {
        return;
    }
    setTile(M[0x91], 0x2B);
    setTile((M[0x91] + 1) & 0xFF, 0x2B);
    M[0x91] = (step + M[0x91]) & 0xFF;
    if (!(M[0x91] & 0x80)) {
        setTile(M[0x91], 0x2A);
        setTile((M[0x91] + 1) & 0xFF, 0x2A);
    }
    liftEnd();
}

// $970E – położenie gracza na windzie, przejście piętra
void Misja::liftEnd() {
    liftPlayerY();
    const int x = M[0x89];
    const int y = M[0x91];
    if (y >= 0x90) {
        M[0xB100 + x] = 0x7C;
        M[0xB110 + x] = 0xFC;
        M[0x91] = (y & 0x0F) | 0x10;
        return;
    }
    if (y < 0x10) {
        M[0xB100 + x] = 0x7C;
        M[0xB0F0 + x] = 0xFC;
        M[0x91] = (y & 0x0F) | 0x70;
        return;
    }
    if (M[0x91] != M[0x90]) return;
    if (!(M[0x8F] & 1)) M[0xCB] = 0;
}

// $9757
void Misja::liftPlayerY() {
    const int r = (M[0x91] & 0xF0) + ((M[0x8F] & 1) << 3);
    M[0xBA] = (r + 5 + (r > 0xFF ? 1 : 0)) & 0xFF;
}

// $9771 – drzwi otwierane przełącznikiem ($8D = '1'-'8')
void Misja::switch9771() {
    const int x = (M[0x8D] - 0x31) & 0xFF;
    if (M[0x9E6C + x] != 0) {
        M[0x8F] = 1;
        return;
    }
    if ((M[0xD6] & 0x0F) == 0) M[0x8F]++;
}

// $978A – przełącznik ($8D = $B1-$B8)
void Misja::switch978A() {
    const int a = (M[0x8D] - 0xB1) & 0xFF;
    if (a >= 8) return;
    M[0x80] = a;
    if (M[0x9E6C + a] != 0) {
        M[0x8F] = 1;
        return;
    }
    if ((M[0xD6] & 7) == 0) M[0x8F]++;
    if (playerTouches(M[0x90])) return;
    M[0x9E6C + M[0x80]] = 1;
    sound(0x0A);
    addScore(0x99);
    addScore(0x01);
    if (M[0x80] != 2) return;
    // $97CC – sprawdzenie bloku DCB ($030A/$030B); wynik używany tylko w nieczynnym kodzie
    M[0x80]++;
    M[0x81] = M[0x80];
    const int p = M.word(0x80);
    const int a2 = ((M[p + 8] << 1) & 0xFF) ^ M[p + 7];
    if (a2 != 0xDB) M[0x9E6B]++;
}

// $97E2 – czy gracz dotyka kafla X; false (C = 0) = dotyka
bool Misja::playerTouches(int x) {
    M[0x82] = x;
    if (tileAt((M[0xB9] + 1) & 0xFF, M[0xBA]).y == M[0x82]) return false;
    M[0x85] = (M[0xB9] + 6) & 0xFF;
    if (tileAt(M[0x85], M[0xBA]).y == M[0x82]) return false;
    const int px = (M[0x85] - 2) & 0xFF;
    M[0x85] = (M[0xBA] + 0x13) & 0xFF;
    if (tileAt(px, M[0x85]).y == M[0x82]) return false;
    if (tileAt((M[0xB9] + 3) & 0xFF, M[0x85]).y == M[0x82]) return false;
    return true;
}

// $9826 – dane elementu specjalnego $8D (tablica $9DAB, rekordy po $9DAA bajtów)
void Misja::specialSetup() {
    M.setWord(0x80, 0x9DAB);
    int y = 0;
    for (;;) {
        const int a = M[M.word(0x80)];
        if (a == M[0x8D]) break;
        if (a == 0x20) return;
        M.setWord(0x80, M.word(0x80) + M[0x9DAA]);
    }
    const int p = M.word(0x80);
    M[0x8D] = M[p];
    y = 1;
    for (int x = 0; x < 6; x += 2) {
        liftGraphics(M[p + y]);
        M[0x9E74 + x] = M[0x86];
        M[0x9E75 + x] = M[0x87];
        y++;
    }
    M[0x122A] = M[p + y] | 0xF0;
    y++;
    M[0x122B] = M[p + y];
    y++;
    M[0x96C4] = M[p + y];
    y++;
    M[0x96C5] = M[p + y];
    y++;
    M[0x96A6] = M[p + y];
    y++;
    M[0x96A7] = M[p + y];
    for (int x = 1; x < 0x80; ++x) {
        M[0x90] = x;
        if (M[0xBA00 + x] != 0x2A) continue;
        if (M[0xCB] == 0x0C || M[0xCB] == 0x0D) return;
        M[0x91] = x;
        M[0x8F] = 0;
        return;
    }
}

// $98A7 (JSR $969C) – znaki windy / drzwi w zestawach znaków
void Misja::rooms98A7() {
    if (M[0x8D] == 0x20) return;
    liftCharCopy(M[0x8F] & 1, 0x0F);
    if (M[0x8D] >= 0x31 && M[0x8D] < 0x3A) {
        // $98E8 – migające drzwi
        int x = M[0xD6];
        for (int y = 0x0F; y >= 0; --y) {
            x &= 3;
            int a = M[0x9908 + x];
            if (y >= 8) {
                a &= 0xFC;
                if (a == 0) a &= 0x3F;
            } else {
                a &= 0x3F;
            }
            M[0x52B0 + y] = a;
            M[0x56B0 + y] = a;
            x--;
        }
        return;
    }
    liftCharCopy(2, 0x1F);
}

// $98C4 – 16 linii grafiki nr A ($9E74) do znaków $52A0/$56A0 (od X w dół)
void Misja::liftCharCopy(int a, int x) {
    const int y0 = a << 1;
    M[0x86] = M[0x80] = M[0x9E74 + y0];
    M[0x87] = M[0x9E75 + y0];
    M[0x81] = (M[0x87] + 4) & 0xFF;
    for (int y = 0x0F; y >= 0; --y) {
        M[0x52A0 + x] = M[M.word(0x86) + y];
        M[0x56A0 + x] = M[M.word(0x80) + y];
        x--;
    }
}

// $990C – $86/$87 = $4800 + (A & $7F) × 16
void Misja::liftGraphics(int a) {
    const int v = 0x4800 + ((a & 0x7F) << 4);
    M.setWord(0x86, v);
}

// $9925 – winda: szyb ($2B) nad i pod platformą ($2A)
void Misja::lift9925() {
    int x = M[0x90];
    if (x != 0) {
        M[0xBA00 + x] = 0x2B;
        M[0xBA01 + x] = 0x2B;
    }
    x = M[0x91];
    M[0xBA00 + x] = 0x2A;
    M[0xBA01 + x] = 0x2A;
    while (!shaftStep(0x2A, x, 0x10)) {}
    x = M[0x91];
    while (!shaftStep(1, x, -0x10)) {}
    int a = 0;
    const int r = M[0x89];
    if (r >= 0x10 && M[0xB0F0 + r] == 0x7C) a |= 0x80;
    if (r < 0xF0 && M[0xB110 + r] == 0x7C) a |= 0x01;
    M[0x8E] = a;
}

// $996A – drzwi
void Misja::door996A() {
    const int x = (M[0x8D] - 0x31) & 0xFF;
    if (M[0x9E6C + x] == 0) {
        fillDown(0, M[0x90]);
        return;
    }
    if (x < 7) return;
    M[0xBA00 + M[0x90]] = 0;
}

// $998B / $9995 – zapełnianie w dół
void Misja::fillDown(int a, int x) {
    while (!shaftStep(a, x, 0x10)) {}
}

// $9999 / $99A0 – kafel szybu $AB o wiersz niżej/wyżej (X zmieniany); true (C) = koniec
bool Misja::shaftStep(int a, int& x, int dir) {
    x = (x + dir) & 0x7F;
    const int code = M[0xBA00 + x] & 0x3F;
    if (code == 0x2A) return true;
    if (M[0x1200 + code] & 0x0F) return true;
    M[0xBA00 + x] = 0xAB;
    if (a != 0) M[0xBA01 + x] = 0xAB;
    return false;
}

// $99CB – animacje i działanie kafli specjalnych (procedury z tablicy $9D61)
void Misja::tileAnimations() {
    for (M[0x8A] = 0; M[0x8A] < 0x80; M[0x8A]++) {
        int x = M[0x8A];
        int a = M[0x1200 + (M[0xBA00 + x] & 0x3F)] & 0xF0;
        if (a < 0x10) continue;
        const int y = a >> 3;
        M[0x9A01] = M[0x9D61 + y];
        M[0x9A02] = M[0x9D62 + y];
        M[0x8B] = M[0x8A] & 0x0F;
        M[0x8C] = M[0x8A] >> 4;
        const int carry = (M[0x8A] >> 3) & 1;   // C po czterech LSR
        switch (M.word(0x9A01)) {            // JSR $xxxx (samomodyfikujący się)
        case 0x9A1F: tile9A1F(x); break;
        case 0x9A6C: tile9A6C(carry); break;
        case 0x9A87: tile9A87(carry); break;
        case 0x9A9B: tile9A9B(carry); break;
        case 0x9B0C: tile9B0C(x); break;
        case 0x9B52: tile9B52(); break;
        case 0x9B6C: tileDebris(7, 1); break;
        case 0x9B72: tileDebris(1, 6); break;
        case 0x9C3F: tile9C3F(); break;
        case 0x9BF1: tile9BF1(x, carry); break;
        case 0x9B9B: tile9B9B(); break;
        case 0x9BA9: tile9BA9(x); break;
        default: break;                  // $96B1: RTS
        }
        x = M[0x8A];
        a = M[0x1200 + (M[0xBA00 + x] & 0x3F)] & 0xF0;
        if (a != 0xC0) setTile(x, M[0xBA00 + x]);
    }
}

// $9A1F – migająca ściana (bit 6 kafla); znaki $2C/$32 pod spodem zamieniane na $D0/$D1
void Misja::tile9A1F(int x) {
    int y = ((x << 1) + 0x20) & 0xFF;
    if (M[0xBA00 + x] & 0x40) {
        const int screen = M.word(0x94);
        for (;;) {
            const int c = M[(screen + y) & 0xFFFF] & 0x7F;
            int a;
            if (c == 0x2C) a = 0xD0;
            else if (c == 0x32) a = 0xD1;
            else break;
            M[(screen + y) & 0xFFFF] = a;
            y += 0x20;
            if (y > 0xFF) break;
        }
    }
    // $9A45
    const int k = (M[0x8B] - 1) & 0xFF;
    const int carry = (k >> 5) & 1;   // C po trzech ASL
    const int a = (((k << 3) & 0xFF) + M[0xD6] + carry) & 0xFF;
    if (a & 0x3F) return;
    M[0xBA00 + M[0x8A]] ^= 0x40;
    if (M[0x9B51] == 0) return;
    // pułapka zabezpieczenia ($9B51 ustawiane w $9B4C)
    M[0xAC00 + M.random()]++;
}

// $9A6C
void Misja::tile9A6C(int carry) {
    tileSpawn(4, 4, carry);
    int a;
    bool c;
    if (!tileFrame7(a, c)) return;
    int x = 0xA2;
    if (c) {
        if (a < 3) return;
        x = 0xA3;
    }
    M[0xBA00 + M[0x8A]] = x;
}

// $9A87
void Misja::tile9A87(int carry) {
    tileSpawn(0, 5, carry);
    int a;
    bool c;
    if (!tileFrame7(a, c)) return;
    if (!c) return;
    M[0xBA00 + M[0x8A]] = 0xA1;
}

// $9A9B
void Misja::tile9A9B(int carry) {
    tileSpawn(7, 3, carry);
    int a;
    bool c;
    if (!tileFrame7(a, c)) return;
    if (c && a >= 3) return;
    M[0xBA00 + M[0x8A]] = 0xA1;
}

// $9AB3 – co 16 ramek (ramka 7): A = kolumna gracza + 1 - kolumna kafla, C = A >= 0.
// false = PLA PLA RTS (wyjście z procedury wywołującej)
bool Misja::tileFrame7(int& a, bool& c) {
    if ((M[0xD6] & 0x0F) != 7) return false;
    const int v = (M[0xB9] >> 3) + 1;
    c = v >= M[0x8B];
    a = (v - M[0x8B]) & 0xFF;
    return true;
}

// $9ACA – odłamek z kafla (rodzaj Y, przesunięcie X)
void Misja::tileSpawn(int x, int y, int carry) {
    if (((M[0xD6] + M[0x8B] + carry) & 7) != 0) return;
    if (M.random() & 3) return;
    M[0x84] = x;
    M[0x85] = y;
    const int px = (((M[0x8B] << 3) & 0xFF) + M[0x84]) & 0xFF;
    const int py = (((M[0x8C] << 3) & 0xFF) + 4) & 0xFF;
    spawnDebris(M[0x85], px, py);
    M[0x81] = 3;
    int a = M[0x82];
    for (int k = 7; k >= 0; --k) {
        const int c = (a >> 7) & 1;
        a = (a << 1) & 0xFF;
        M[0x83] = (M[0x83] >> 1) | (c << 7);
    }
    M[0x80] = a;
    M[0x9E6A] = M[M.word(0x80) + 3] & 0x7F;
}

// $9B0C – zapadnia
void Misja::tile9B0C(int x) {
    if (playerTouches(x)) return;
    M[0x92] = 0;
    M[0xBA00 + M[0x8A]] = 0x26;
    M[0xCC] = 0;
    M[0xCB] = 5;
    M[0xCD] = 0x0A;
    sound(1);
    damage(0x12);
    // zabezpieczenie: bufor kasety $0400 musi być równy kopii w $0580
    M[0x82] = 0x80;
    M[0x80] = 0;
    M[0x83] = 5;
    M[0x81] = 4;
    for (int y = 0x7F; y != 0; --y) {
        if (M[0x0400 + y] != M[0x0580 + y]) {
            M[0x9B51] = (M[0x9B51] >> 1) | 0x80;
            return;
        }
    }
}

// $9B52 – działko w ścianie
void Misja::tile9B52() {
    if (M.random() & 0x3F) return;
    const int x = (M[0x8A] & 0x0F) << 1;
    const int y = M[0x8A] & 0x70;
    spawnObject(M.random() & 1, x, y);
}

// $9B6C / $9B72 – odłamki (np. kapiąca woda, iskry)
void Misja::tileDebris(int kind, int dx) {
    if (M[0xD6] & 7) return;
    if (M.random() & 7) return;
    M[0x84] = dx;
    const int px = (((M[0x8B] << 3) & 0xFF) + M[0x84]) & 0xFF;
    const int py = (((M[0x8C] << 3) & 0xFF) + 3) & 0xFF;
    spawnDebris(kind, px, py);
}

// $9B9B
void Misja::tile9B9B() {
    if (M[0x92] < 2) return;
    setTile(M[0x8A], 0x16);
}

// $9BA9
void Misja::tile9BA9(int x) {
    if (M[0x93] < 2) return;
    tile9A1F(x);
    setTile(M[0x8A], 0x10);
}

// $9BBA – znaki szybu windy (przesuwane liczniki $92/$93)
void Misja::liftChars() {
    liftCharsCopy((M[0x92] + M[0x9E7A]) & 0xFF, 0x0F);
    liftCharsCopy((M[0x93] + M[0x9E7B]) & 0xFF, 0x1F);
}

// $9BCD
void Misja::liftCharsCopy(int a, int x) {
    liftGraphics(a);
    M[0x80] = M[0x86];
    M[0x87] = (M[0x87] + 0xF8) & 0xFF;
    M[0x81] = (M[0x87] + 4) & 0xFF;
    for (int y = 0x0F; y >= 0; --y) {
        M[0x5260 + x] = M[M.word(0x86) + y];
        M[0x5660 + x] = M[M.word(0x80) + y];
        x--;
    }
}

// $9BF1 – przedmiot do zebrania (miga co 8 ramek)
void Misja::tile9BF1(int x, int carry) {
    int t = M[0xBA00 + x];
    const int r1 = M[0xD6] + M[0x8B] + carry;
    const int r2 = (r1 & 0xFF) + M[0x8C] + (r1 > 0xFF ? 1 : 0);
    if (!(r2 & 8)) t = 0x16;
    drawTileAt(t, (M[0x8A] << 1) & 0xFF);
    M[0x85] = (M[0xBA] + 0x1A) & 0xFF;
    bool touch = tileAt((M[0xB9] + 1) & 0xFF, M[0x85]).y == M[0x8A];
    if (!touch) touch = tileAt((M[0xB9] + 6) & 0xFF, M[0x85]).y == M[0x8A];
    if (!touch && playerTouches(M[0x8A])) return;
    const int k = M[0x8A];
    const int item = M[0xBA00 + k];
    M[0xBA00 + k] = 0x16;
    pickUp(item);
}

// $9C3F – miejsce na pojemnik z uranem (centrum bazy)
void Misja::tile9C3F() {
    int y = 3;
    for (; y >= 0; --y) {
        const int a = M[0x9C9E + y];
        if (a == 0) break;
        if (a == M[0x8A]) {
            // pojemnik już stoi
            M[0xBA5E] = 0x1E;
            M[0xBA6E] = 0x1E;
            drawTileAt(0xBB, ((M[0x8A] << 1) - 0x20) & 0xFF);
            return;
        }
    }
    if (y < 0) return;
    M[0xBA5E] = 0x0F;
    M[0xBA6E] = 0x1F;
    M[0x86] = y;
    if (playerTouches(M[0x8A])) return;
    if (M[0xE9] == 0) return;
    M[0xE9]--;
    M[0x9C9E + M[0x86]] = M[0x8A];
    M[0x9CA2]++;
    if (M[0x9CA2] >= 4) M[0xEA]--;        // wszystkie cztery: rusza odliczanie czasu
    sound(0x0C);
    for (int x = 3; x != 0; --x) addScore(0x80);
}

// $9CA3 – kafel A w miejscu X (mapa i ekran)
void Misja::setTile(int x, int a) {
    int y = x & 0x7F;
    M[0xBA00 + y] = a;
    drawTileAt(a, (y << 1) & 0xFF);
}

// $9CB1 – dwa znaki kafla A na ekranie ($94) w miejscu Y
void Misja::drawTileAt(int a, int y) {
    const int v = (a & 0x80) | ((a << 1) & 0x7E);
    const int screen = M.word(0x94);
    M[(screen + y) & 0xFFFF] = v;
    M[(screen + ((y + 1) & 0xFF)) & 0xFFFF] = v | 1;
}

// $9CBE – zniszczalne kafle pomieszczenia: odtworzenie stanu z $B000
void Misja::roomSwitchesInit() {
    M[0x84] = M[0xB000 + M[0x89]];
    M[0x85] = 1;
    M[0x82] = 0;
    for (int x = 0; x < 0x80; ++x) {
        const int a = M[0x1200 + (M[0xBA00 + x] & 0x3F)] & 0xF0;
        if (a != 0 && a < 0xD0) {
            // $9CF4
            int v;
            if (!(M[0x84] & 0x80)) {
                M[0x84] |= M[0x85];
                v = x;
            } else if (M[0x85] & M[0x84]) {
                v = x;
            } else {
                M[0xBA00 + x] = a >= 0x50 ? 0x16 : 0x10;
                v = 0;
            }
            M[0x9D81 + M[0x82]] = v;
            M[0x85] = (M[0x85] << 1) & 0xFF;
            M[0x82]++;
        }
        if (M[0x82] >= 7) break;
    }
    M[0xB000 + M[0x89]] = M[0x84] | 0x80;
}

// $9D21 (JSR $9696) – zapamiętanie zniszczeń przy wyjściu
void Misja::rooms9D21() {
    M[0x84] = M[0xB000 + M[0x89]];
    M[0x85] = 0xFE;
    for (int x = 0; x < 7; ++x) {
        if (M[0x84] != 0) {
            const int y = M[0x9D81 + x];
            const int a = M[0x1200 + (M[0xBA00 + y] & 0x3F)] & 0xF0;
            if (a == 0 || a >= 0xD0) M[0x84] &= M[0x85];
            M[0x85] = ((M[0x85] << 1) | 1) & 0xFF;
        }
        M[0x9D81 + x] = 0;
    }
    M[0xB000 + M[0x89]] = M[0x84];
}

// $9D89 (JSR $9693) – początek gry
void Misja::rooms9D89() {
    for (int x = 7; x >= 0; --x) M[0x9E6C + x] = 0;
    int x = 0;
    do {
        M[0xB100 + x] = M[0x9F00 + x];
        M[0xB000 + x] = 0;
        if (x < 5) M[0x9C9E + x] = 0;
        x = (x - 1) & 0xFF;
    } while (x != 0);
}
