//
//  MisjaGame.cpp
//  Misja (C++ / SDL3)
//
//  Przebieg gry ($7400-$7F5F): przygotowanie, główna pętla (co 2 ramki), przejścia między
//  pomieszczeniami, rozpakowanie pomieszczenia, panel stanu (energia, czas, granaty,
//  amunicja, wynik, mapa), obrażenia, przedmioty, koniec gry, zakończenie i DLI gry.
//
//  Ważniejsze komórki:
//  $89 pomieszczenie (bity 4-7 wiersz, 0-3 kolumna)   $96 poprzednie pomieszczenie
//  $B9/$BA pozycja gracza   $CB stan gracza   $D6 licznik ramek pętli
//  $E4 energia   $E5 zmiana energii   $E6 czas ochronny po trafieniu   $E7 amunicja
//  $E8 granaty   $E9 pojemniki   $EA czas (BCD)   $EC-$EE rekord   $EF-$F1 wynik (BCD)
//  $F2 bit 7 = pokaz
//

#include "Misja.h"

// $747F (JSR $7400) – gra (demo = C). Zwraca Y – przyczynę końca.
int Misja::playGame(bool demo) {
    M[0xF2] = demo ? 0x80 : 0;
    M.waitFrame();
    M[0x22F] = 0;
    M.waitFrame();
    const int last = (M[0x7E75] & 0x80) ? 0xEE : 0xEB;
    for (int x = last; x >= 0x80; --x) M[x] = 0;      // DEX; BMI – tylko $80..
    M[0x2D9] = 0;
    M[0xEF] = M[0xF0] = M[0xF1] = 0;
    M[0xE5] = 0x77;
    M[0xE7] = 0x50;
    M[0xE8] = 0x09;
    M[0xEA] = 0x65;
    if (M[0x7E75] & 0x80) {
        for (int x = 0x1F; x >= 0; --x) M[0xBAE0 + x] = M[0x1200 + x];
    }
    for (int a = 0xBB00; a < 0xC000; ++a) M[a] = 0;
    M[0x80] = 0;
    M[0x81] = 0xC0;
    for (int y = 0xDF; y >= 0xC0; --y) {
        M[0xBC00 + y] = 0xFF;
        M[0xBE00 + y] = 0xFF;
        M[0xBB00 + y] = 0xAA;
    }
    M[0xBBBF] = 0xAA;
    M[0xBBE0] = 0xAA;
    for (int x = 0x5F; x >= 0; --x) M[0xBA80 + x] = 0x54;
    M.waitFrame();
    M.setWord(0x200, 0x7D99);
    M.antic(0x0E, 0xC0);
    M.setWord(0x58, 0xAE00);
    M.setWord(0x94, 0xAF00);
    M.setWord(0x230, 0x741C);
    for (int x = 8; x >= 0; --x) M[0x744C + x] = M[0x746D + x];
    M[0x741D] = M[0x741E] = 0x70;
    M[0x741F] = 0x30;
    M[0x26F] = 0x31;
    for (int x = 8; x >= 0; --x) {
        M[0x2C0 + x] = M[0x6807 + x];
        if (x < 8) {
            M[0x740C + x] = 0xBF;
            M[0x7414 + x] = 0xBF;
        }
    }
    M.gtia(0x1D, 3);
    M.antic(0x07, 0xB8);
    M.waitFrame();
    M[0xBA] = 0x20;
    M[0xB9] = 0x28;
    M[0xCB] = 7;
    M[0xCE] = 0x10;
    M[0xD0] = 0x10;
    rooms9D89();
    objects9214();
    enterRoom();
    statusPanel();
    {
        const int src = M.word(0x58), dst = M.word(0x94);
        int y = 0;
        do {
            M[dst + y] = M[src + y];
            y = (y - 1) & 0xFF;
        } while (y != 0);
        const int a = M[src + 1];
        M[0x2FC] = 0xFF;
        M[0x11] = a;
        M.hitclr();
        M[0x7E75] = a;
    }
    M.waitFrame();
    if (!(M[0xF2] & 0x80)) return mainLoop(0);

    // pokaz: 8 losowych pomieszczeń
    for (int x = 7; x >= 0; --x) M[0x75F8 + x] = M[0x6805];
    for (int x = 7; x >= 0; --x) {
        int a;
        for (;;) {
            a = M.random();
            if (a == 0) continue;
            M[0x89] = a;
            bool used = false;
            for (int k = 7; k >= 0; --k) if (a == M[0x75F8 + k]) used = true;
            if (used) continue;
            if (a >= M[0x6805] && a < M[0x6806]) continue;
            if (roomPtr()) continue;
            break;
        }
        enterRoom();
        mainLoop(1);
        if (!(M[0xF2] & 0x80)) return -1;   // SEC; RTS – pokaz przerwany
        M[0xD6]++;
        M[0x75F8 + x] = M[0x89];
    }
    return -1;   // CLC; RTS
}

// $7600 – główna pętla. entry: 0 – od $7600, 1 – od $762E (pokaz), 2/3 – w oczekiwaniu przy
// $765D / $7672 (wznowienie zapisu). Zwraca Y przy końcu gry.
int Misja::mainLoop(int entry) {
    bool carry = false;
    if (entry == 1) goto tail;
    if (entry == 2) goto wait;
    if (entry == 3) goto waitLine;
    for (;;) {
        swapScreens();
        objects9202();
        rooms98A7();
        if (!(M[0xF2] & 0x80)) {
            player7F6C();
            roomExit();
        }
        carry = enemies88B2();
        if ((M[0xF2] & 0x80) && M[0xE5] == 0) {
            M[0xB9] = M.random() & 0x7F;
            M[0xBA] = ((M.random() & 0x7F) + 0x15 + (carry ? 1 : 0)) & 0xFF;
        }
    tail:
        applyCheats();
        copyRoomToScreen();
        statusPanel();
        rooms969F();
        objects91C8();
        animateChars();
        applyCheats();
        {
            int reason = 0;
            if (!checkEnd(reason)) return reason;
        }
        M[0xD6]++;
        if (!(M[0x97] & 0x80)) M[0x97]++;
        {
            const int a = (M.collision(4) | M.collision(5)) & 0x08;
            M.hitclr();
            if (a) damage(1);
        }
    wait:
        M.setParkTag(1);
        M.waitUntil([this] { return M[0x21C] == 0; });
        M.setParkTag(0);
        M[0x21C] = 2;
        M[0x4D] = 0;
        M[0x22F] = 0x3D;
    waitLine:
        M.setParkTag(2);
        M.waitVcount(0x52);
        M.setParkTag(0);
    }
}

// Ułatwienia z menu (nie ma ich w oryginale): pełna energia, amunicja, granaty – tylko w grze
void Misja::applyCheats() {
    if (M[0xF2] & 0x80) return;
    if (infiniteEnergy) {
        M[0xE4] = 0x77;
        if (M[0xE5] & 0x80) M[0xE5] = 0;   // nie ubywa energii
    }
    if (infiniteAmmo) M[0xE7] = 0x50;
    if (infiniteGrenades) M[0xE8] = 0x09;
}

// $767A – wyjście za krawędź pomieszczenia
void Misja::roomExit() {
    M[0x84] = M[0x89];
    int a = M[0xB9];
    int y;
    if (a == 0x78) {
        M[0xB9] = 1;
        y = (M[0x89] + 1) & 0xFF;
        if (y == 0) {           // INY; BNE – przy $FF dalej jak w lewo
            M[0xB9] = 0x77;
            y = (M[0x89] - 1) & 0xFF;
        }
    } else if (a == 0) {
        M[0xB9] = 0x77;
        y = (M[0x89] - 1) & 0xFF;
    } else {
        a = M[0xBA];
        if (a < 0x15) {
            if (M[0xCB] < 8) return;
            M[0x89] = (M[0x89] - 0x10) & 0xFF;
            M[0xBA] = 0x90;
            enterRoom();
            return;
        }
        if (a >= 0x91) {
            M[0x89] = (M[0x89] + 0x10) & 0xFF;
            M[0xBA] = 0x16;
            enterRoom();
        }
        return;
    }
    M[0x89] = (M[0x89] & 0xF0) | (y & 0x0F);
    enterRoom();
}

// $76D3 – wejście do pomieszczenia $89 (z $84)
void Misja::enterRoom() {
    M[0xD6] = 0;
    const int newRoom = M[0x89];
    M[0x89] = M[0x84];
    const int oldRoom = M[0x84];
    objects9325();
    rooms9D21();
    M[0x96] = oldRoom;
    M[0x89] = newRoom;
    unpackRoom();
    rooms96B2();
    objects924B();
    copyRoomToScreen();
    playerReset();
    M[0x97] = 0;
    M.waitVcount(0x4E);
    int y = 7;
    const int a = M[0x89];
    if (a >= M[0x6805] && a < M[0x6806]) y = 0x15;
    if (a == M[0x6804]) y = 0x19;
    for (int x = 3; x >= 0; --x, --y) M[0x2C4 + x] = M[0x6807 + y];
    swapScreens();
    rooms98A7();
    roomCharsets();
}

// $7731
void Misja::playerReset() {
    player851F();
    enemies8AE8();
}

// $7737 – rozpakowanie pomieszczenia do $BA00 (128 kafli)
void Misja::unpackRoom() {
    while (roomPtr()) {
        // Brak też poprzedniego pomieszczenia: na Atari pętla bez końca (gra staje, przerwania
        // działają dalej). Tu wątek gry czeka bez końca, żeby okno i RESET nadal działały.
        if (M[0x89] == M[0x96]) M.waitUntil([] { return false; });
        M[0x89] = M[0x96];
    }
    const int p = M.word(0x80);
    int y = 0, x = 0;
    do {
        const int b = M[(p + y) & 0xFFFF];
        const int a = ((b & 0x7F) >> 1) | (b & 0x80);
        M[0x84] = a;
        M[0xBA00 + x] = a;
        x = (x + 1) & 0xFF;
        if (b & 1) {
            y = (y + 1) & 0xFF;
            int count = (M[(p + y) & 0xFFFF] - 1) & 0xFF;
            do {
                M[0xBA00 + x] = M[0x84];
                x = (x + 1) & 0xFF;
                count = (count - 1) & 0xFF;
            } while (count != 0);
        }
        y = (y + 1) & 0xFF;
    } while (!(x & 0x80));
}

// $7770 – adres danych pomieszczenia $89 w $80/$81; C = brak pomieszczenia
bool Misja::roomPtr() {
    const int x = M[0x89];
    M[0x81] = M[0xAC00 + x];
    if (M[0x81] == 0) return true;
    M[0x80] = M[0xAB00 + x];
    return false;
}

// $7781 – mapa kafli $BA00 -> ekran ($94): kafel = 2 znaki
void Misja::copyRoomToScreen() {
    const int dst = M.word(0x94);
    int y = 0;
    for (int x = 0; x < 0x80; ++x) {
        const int b = M[0xBA00 + x];
        const int a = (b & 0x80) | ((b << 1) & 0x7E);
        M[(dst + y) & 0xFFFF] = a;
        y = (y + 1) & 0xFF;
        M[(dst + y) & 0xFFFF] = a | 1;
        y = (y + 1) & 0xFF;
    }
}

// $7799 – zamiana ekranów ($58 <-> $94) i adresy LMS listy wyświetlania
void Misja::swapScreens() {
    for (int x = 1; x >= 0; --x) {
        const int t = M[0x94 + x];
        M[0x94 + x] = M[0x58 + x];
        M[0x58 + x] = t;
    }
    const int a = M[0x59];
    for (int x = 0x2F; x >= 0; x -= 3) M[0x7422 + x] = a;
    if (M[0x89] != M[0x6804]) return;
    const int s = M.word(0x58);
    M[s + 0x86] = 0x10;
    M[s + 0x88] = 0x10;
    M[s + 0x90] = 0x10;
}

// $77D1 – zestawy znaków zależne od rodzaju pomieszczenia (zamiana bloków pamięci)
void Misja::roomCharsets() {
    int a = 0;
    const int r = M[0x89];
    if (r == M[0x6804]) a = 1;
    if (r >= M[0x6805] && r < M[0x6806]) a = 2;
    if (a == M[0x78B5]) return;
    M[0x84] = a;
    int x = 3;
    do { --x; } while (a != M[0x78B5 + x]);
    M[0x85] = M[0x78B5];
    M[0x784A] = M[0x7851] = M[0x78B8];
    M[0x784B] = M[0x7852] = M[0x78B9];
    M[0x78B5] = M[0x84];
    M[0x78B5 + x] = M[0x85];
    int y = x * 2;
    M[0x7847] = M[0x784D] = M[0x78B8] = M[0x78B8 + y];
    M[0x7848] = M[0x784E] = M[0x78B9] = M[0x78B9 + y];
    M[0x78B8 + y] = M[0x784A];
    M[0x78B9 + y] = M[0x784B];
    for (x = 3; x >= 0; --x) {
        // pętla z samomodyfikującymi się adresami ($7846-$7854)
        for (y = 0x7F; y >= 0; --y) {
            const int vx = M[M.word(0x7847) + y];
            const int va = M[M.word(0x784A) + y];
            M[M.word(0x784D) + y] = va;
            M[M.word(0x7851) + y] = vx;
        }
        const int steps = x == 2 ? 3 : 1;
        for (int k = 0; k < steps; ++k) {
            M[0x7848]++; M[0x784E]++; M[0x784B]++; M[0x7852]++;
        }
    }
    y = M[0x78B5];
    if (y == 0) {
        for (x = 0x1F; x >= 0; --x) M[0x1200 + x] = M[0xBAE0 + x];
        return;
    }
    for (x = 0x1F; x >= 0x18; --x) M[0x1200 + x] = 0;
    for (x = 0x0F; x >= 0x08; --x) M[0x1200 + x] = 0;
    if (y != 1) {
        M[0x120F] = 4;
        M[0x121F] = 4;
    }
}

// $78BE – animacja znaków ($5280, $5288, $5680, $5688)
void Misja::animateChars() {
    int x = M[0xD6] & 7;
    for (int y = 7; y >= 0; --y) {
        for (int base : {0x5280, 0x5288, 0x5680, 0x5688}) {
            M[base + y] = (M[base + y] & 0xFC) | M[0x78FA + x];
        }
        x = (x + 1) & 7;
    }
}

// $7902 – panel stanu
void Misja::statusPanel() {
    if (M[0x6803] != 0) {          // hasło: nieskończona amunicja i granaty
        M[0xE7] = 0x50;
        M[0xE8] = 0x09;
    }
    if (M[0xEA] < 0x65) {
        if (M[0xEB] == 0) {
            M[0xEB] = 0x32;
            M[0xEA]--;
            sound(4);
        }
        M[0xEB]--;
    }
    int y = 0x5A;
    for (int x = 0; x < 6; ++x) {
        printBCD(M[0xEC + x], y);
        if (x + 1 == 3) y -= 2;
    }
    M[0xBAA0] = 0xE2;
    M[0xBAA1] = 0xE3;
    y = 0x25;
    printNumber(M[0xE7], y);
    M[0xBAA8] = 0xE0;
    M[0xBAA9] = 0xE1;
    y = 0x2D;
    printNumber(M[0xE8], y);
    if (M[0xEA] >= 0x65) {
        // pojemniki z uranem
        y = 7;
        for (int x = 3; x >= 0; --x) {
            const int a = x >= M[0xE9] ? 0xD8 : 0xDA;
            M[0xBAB0 + y] = a;
            M[0xBAB1 + y] = a | 1;
            y -= 2;
        }
    } else {
        // czas
        const int x = (M[0x6806] - 2) & 0xFF;
        if (M[0x89] >= M[0x6805] && x >= M[0x89]) M[0x2C7]++;
        M[0xBAB0] = M[0xBAB1] = M[0xBAB8] = 0x54;
        M[0x84] = (M[0x14] & 0x10) ? 0xFF : 0;
        M[0xBAB2] = (0xE4 & M[0x84]) ? 0xE4 : 0x54;
        M[0xBAB3] = (0xE5 & M[0x84]) ? 0xE5 : 0x54;
        y = 0x37;
        printNumber(M[0xEA], y);
    }
    // $79C2 – mapa
    for (y = 3; y >= 0; --y) {
        M[0xBABC + y] = 0xD6;
        M[0xBADC + y] = 0xD7;
    }
    M[0x87] = 3;
    y = M[0x7E7F];
    M[0x86] = 0;
    M[0xBD00 + y] = M[0xBD01 + y] = M[0xBF00 + y] = M[0xBF01 + y] = 0;
    M[0x84] = (M[0x89] & 0x08) >> 2;
    M[0x81] = 0xBC + M[0x84];
    M[0x83] = 0xBD + M[0x84];
    M[0x80] = M[0x82] = 0;
    y = (((M[0x89] & 0xF0) >> 3) + 0xC0) & 0xFF;
    M[0x7E7F] = y;
    const int bit = 0x80 >> (M[0x89] & 7);
    const int p80 = M.word(0x80), p82 = M.word(0x82);
    M[p82 + y] = bit;
    M[p82 + y + 1] = bit;
    const int a = (bit ^ 0xFF) & M[p80 + y];
    if (!(M[0xF2] & 0x80)) {
        M[p80 + y] = a;
        M[p80 + y + 1] = a;
    }
    // $7A36 – zabezpieczenie: wczytanie nie z kasety psuje tablicę pomieszczeń
    if (M[0x89] >= 0x20) {
        const int d = M[M.word(0x86)];
        if (((d ^ 0xF0) & 0xF0) != 0x90) {
            sound(M.random() & 0x1F);
            M[0xAB00 + M.random()]++;
        }
    }
    // $7A56 – energia
    if (M[0xE6] != 0) M[0xE6]--;
    if (M[0xE5] & 0x80) {
        M[0xE5]++;
        if (M[0xE4] != 0) {
            M[0xE4]--;
            if (M[0xE4] < 0x20 && !(M[0xF2] & 0x80)) sound(0x17);
        }
    } else if (M[0xE5] != 0) {
        M[0xE5]--;
        if (M[0xE4] >= 0x77) {
            M[0xE5] = 0;
        } else {
            M[0xE4]++;
            if (M[0xE4] == 0x20 && M[0xE5] < 0x50 && !(M[0xF2] & 0x80)) sound(7);
        }
    }
    // $7AA1 – pasek energii
    M[0xBA9F] = 0x5F;
    M[0xBA80] = 0x55;
    int x = M[0xE4] >> 2;
    int f = 0;
    if (x < 8) {
        const int e5 = M[0xE5];
        if (e5 == 0 || (e5 & 0x80)) f = M[0x14];
    }
    M[0x85] = ((f << 3) & 0x80) ^ 0x80;
    {
        const int k = M[0xE4] & 3;
        const int first = M[0x7B0E + k];
        const int v = M[0x7B0A + k];
        for (y = 4; y >= 0; --y) {
            M[0x4AEB + y] = v;
            M[0x4EE8 + y] = v;
        }
        M[0x4AEB] = first;
    }
    M[0xBA81 + x] = 0x5D | M[0x85];
    {
        int k = x;
        if (M[0xE4] != 0) k++;
        while (k < 0x1E) {
            M[0xBA81 + k] = 0x5E;
            k++;
        }
    }
    for (--x; x >= 0; --x) M[0xBA81 + x] = 0x5C | M[0x85];
}

// $7B12 (JSR $7403) – obrażenia A
void Misja::damage(int a) {
    M[0x84] = a;
    if (a == 1 && M[0xE6] != 0) return;
    M[0xE6] = 3;
    const int x = M[0xE5];
    if (x != 0 && x != 0xFF) return;
    const int v = (0xFF - M[0x84]) & 0xFF;
    if (v >= 0xCF) {
        M[0xE5] = v;
        return;
    }
    M[0xE4] = 0;
}

// $7B3A (JSR $7409) – punkty (A = BCD, młodsza cyfra × 10)
void Misja::addScore(int a) {
    if (M[0xF2] & 0x80) return;
    M[0x84] = a >> 4;
    int c;
    auto add = [&](int x, int y) {
        int lo = (x & 0x0F) + (y & 0x0F) + c;
        if (lo > 9) lo += 6;
        int hi = (x >> 4) + (y >> 4) + (lo > 0x0F ? 1 : 0);
        if (hi > 9) hi += 6;
        c = hi > 0x0F ? 1 : 0;
        return ((hi << 4) | (lo & 0x0F)) & 0xFF;
    };
    c = 0;
    M[0xEF] = add((a << 4) & 0xFF, M[0xEF]);
    M[0xF0] = add(M[0x84], M[0xF0]);
    M[0xF1] = add(0, M[0xF1]);
}

// $7B5E (JSR $7406) – zebranie przedmiotu (kod znaku w A)
bool Misja::pickUp(int a) {
    M.hitclr();
    switch ((a & 0x3F) - 0x38) {
    case 0:                                   // amunicja
        M[0xE7] = 0x50;
        sound(0x0E);
        return false;
    case 1: {                                 // granaty
        int v = 9 + M[0xE8];
        if (v >= 0x63) v = 0x63;
        M[0xE8] = v;
        sound(0x0B);
        return false;
    }
    case 2: {                                 // energia
        refillEnergy();
        return true;
    }
    default:                                  // pojemnik z uranem
        M[0xE9]++;
        sound(0x0C);
        addScore(0x80);
        addScore(0x80);
        return false;
    }
}

// $7B9B – energia +$38 (najwyżej $4F)
void Misja::refillEnergy() {
    int a = M[0xE5];
    if (a & 0x80) a = 0;
    a += 0x38;
    if (a >= 0x50) a = 0x4F;
    M[0xE5] = a;
    sound(0x0D);
}

// $7BB3 – liczba (0-99) jako dwie cyfry w panelu, Y = pozycja
void Misja::printNumber(int a, int& y) {
    int n = a & 0xFF, r = 0;
    for (int i = 0; i < 8; ++i) {
        const int c = (n >> 7) & 1;
        n = (n << 1) & 0xFF;
        // SED: r = r + r + c
        int lo = (r & 0x0F) * 2 + c;
        if (lo > 9) lo += 6;
        int hi = (r >> 4) * 2 + (lo > 0x0F ? 1 : 0);
        if (hi > 9) hi += 6;
        r = ((hi << 4) | (lo & 0x0F)) & 0xFF;
    }
    M[0x84] = n;
    M[0x85] = r;
    printBCD(r, y);
}

// $7BC8 – dwie cyfry BCD (najpierw młodsza)
void Misja::printBCD(int a, int& y) {
    printDigit(a, y);
    printDigit(a >> 4, y);
}

// $7BD1
void Misja::printDigit(int a, int& y) {
    const int v = (((a & 0x0F) + 0x20) << 1) & 0xFF;
    M[0xBA7F + y] = v;
    M[0xBA80 + y] = v | 1;
    y -= 2;
}

// $7BE2 – koniec gry? false = koniec (Y = przyczyna: 0 sukces, 1 energia, 2 czas, 3 przerwanie)
bool Misja::checkEnd(int& y) {
    if (M[0xF2] & 0x80) {
        // pokaz
        if (M[0xD6] == 0xFF) { sound(0x80); return false; }
        if ((M.trig(0) & M.trig(1) & M.consol()) != 0) return true;
        M[0xF2] = 0;
        sound(0x80);
        return false;
    }
    if (M[0x89] == M[0x6804] && M[0xB9] == 0x34) {
        victory();
        y = 0;
        return false;
    }
    y = 1;
    if (M[0xE4] == 0) { sound(0x80); return false; }
    y = 2;
    if (M[0xEA] == 0) {
        timeOut();
        sound(0x80);
        return false;
    }
    y = 3;
    if (M[0x11] == 0) { sound(0x80); return false; }
    int a = M[0x2FC];
    if (a == 0xFF) return true;
    M[0x2FC] = 0xFF;
    if (a == 0x1C) { sound(0x80); return false; }          // ESC
    if (a == 0xDC) throw WarmStart{};                       // CONTROL+SHIFT+ESC: JMP $E474
    if (a != 0x21) return true;                             // spacja – pauza
    if (M[0xCB] >= 0x0C) return true;
    if (M[0xF2] & 0x80) return true;
    M.waitUntil([this] {
        M[0x4D] = 0;
        return M[0x2FC] != 0xFF;
    });
    a = M[0x2FC];
    M[0x2FC] = 0xFF;
    if (a == 0xF5) {
        // CONTROL+SHIFT+5 z wciśniętymi START i SELECT – energia
        if ((M.consol() & 7) == 4) refillEnergy();
        return true;
    }
    M[0x11] = 0x80;
    return true;
}

// $7C70 – ucieczka śmigłowcem (zakończenie)
void Misja::victory() {
    sound(0x80);
    sound(0x13);
    M[0x95] = 0xAE;
    swapScreens();
    M[0x84] = 7;
    playerReset();
    M[0xAEAC] = 0x36;
    M[0xAEAD] = 0x37;
    for (int x = 0x64; x >= 0; --x) helicopterFrame();
    for (int x = 8; x >= 0; --x) M[0x744C + x] = M[0x7476 + x];
    M[0x741F] = 0x10;
    do {
        bool carry;
        do {
            helicopterFrame();
            int a = M[0x741D] - 0x10;
            carry = a >= 0;
            if (!carry) a = 0x70;
            M[0x741D] = M[0x741E] = a & 0xFF;
            M[0x744C] = M[0x744D] = (0x70 - M[0x741D]) & 0xFF;
        } while (carry);
        for (int x = 0; x < 0xC0; ++x) M[0xAE00 + x] = M[0xAE20 + x];
        for (int x = 0x1F; x >= 0; --x) M[0xAEC0 + x] = 0;
        M[0x84]--;
    } while (!(M[0x84] & 0x80));
    sound(0x80);
}

// $7CF1 – dwie ramki z błyskiem nieba i obracającym się wirnikiem śmigłowca
void Misja::helicopterFrame() {
    for (int y = 1; y >= 0; --y) {
        M.waitVcount(6);
        for (int r = 0x16; r <= 0x19; ++r) M.gtia(r, 0);
        M.waitVcount(0x0E);
        for (int x = 3; x >= 0; --x) M.gtia(0x16 + x, M[0x681D + x]);
        M.waitFrame();
    }
    const int x = (M[0x14] >> 1) & 6;
    M[0x7D6D] = M[0x7D6F + x];
    M[0x7D6E] = M[0x7D70 + x];
    M[0x80] = (M[0x14] & 2) ? 0x80 : 0;
    int y = (M[0x84] << 5) & 0xFF;
    if (y < 0x60) return;
    for (int k = 0x0D; k >= 0; --k) {
        M[0xADB3 + y] = M[0x7D61 + k] | M[0x80];
        y--;
        if (k == 7) M[0x80] ^= 0x80;
    }
}

// $7D77 – koniec czasu: kolory sprite'ów rozjaśniają się przez 20 ramek
void Misja::timeOut() {
    for (int y = 0x14; y != 0; --y) {
        M.waitFrame();
        for (int x = 7; x >= 0; --x) {
            const int a = M[0x2C0 + x];
            M[0x2C0 + x] = (a & 0x0F) ? (a + 1) & 0xFF : 0;
        }
    }
}

// $7D99 – DLI gry
void Misja::gameDli() {
    if (M.vcount() < 5) {
        // początek ekranu: pozycje sprite'ów z tablic $740C/$7414
        M.antic(9, 0x50);
        for (int x = 3; x >= 0; --x) {
            M[0x7410 + x] = (M[0x740C + x] + 0x40) & 0xFF;
            M[0x7418 + x] = (M[0x7414 + x] + 0x40) & 0xFF;
            M.gtia(x, 0);
            M.gtia(4 + x, 0);
        }
        for (int x = 8; x >= 0; --x) M.gtia(0x12 + x, M[0x2C0 + x]);
        M[0x00] = 0;
        return;
    }
    int a = M[0x00];
    if (a == 0) {
        for (int x = 3; x >= 0; --x) M.gtia(4 + x, M[0x7418 + x]);
        for (int x = 0; x < 4; ++x) M.gtia(x, M[0x7410 + x]);
        M[0x00]++;
        return;
    }
    if (a < 0x10) {
        M.antic(9, (a & 1) ? 0x54 : 0x50);
        M[0x00]++;
        return;
    }
    if (a < 0x11) {
        for (int x = 0; x < 4; ++x) M.gtia(x, 0);
        M[0x00]++;
        a = M[0x00];
    }
    M.antic(9, (a & 1) ? 0x48 : 0x4C);
    M[0x00]++;
    a = M[0x00];
    if (a == 0x14) {
        for (int x = 8; x >= 0; --x) M.gtia(x, M[0x7E76 + x]);
        return;
    }
    if (a >= 0x18) {
        M.gtia(0, 0);
        M.gtia(1, 0);
    }
    for (int x = 8; x >= 0; --x) M.gtia(0x12 + x, M[0x6810 + x]);
}
