//
//  MisjaTitle.cpp
//  Misja (C++ / SDL3)
//
//  Ekran tytułowy ($6878-$6BDC): strony tekstu z rozjaśnianiem i ściemnianiem kolorów,
//  oczekiwanie na START/fire, sekretne hasło, wynik rekordowy, strony końca gry.
//

#include "Misja.h"

// $6878 – start programu (także po JMP $6800 i JMP $6878)
void Misja::start() {
    for (;;) {
        try {
            title();
        } catch (const Restart&) {
        } catch (const WarmStart&) {
            // Na Atari gorący start systemu kończy grę; tu – jak RESET – od ekranu tytułowego.
            restartState();
        }
    }
}

// Wznowienie gry z zapisu: stan w pamięci, wątek stoi w głównej pętli przy $765D (point 1)
// albo $7672 (point 2).
void Misja::resume(int point) {
    try {
        afterGame(mainLoop(point == 2 ? 3 : 2));
    } catch (const Restart&) {
    } catch (const WarmStart&) {
        restartState();
    }
    start();
}

// Stan sprzętu i systemu jak po RESET (gra nadal w pamięci)
void Misja::restartState() {
    sound(0x80);
    M.setWord(0x224, 0xE462);
    M[0x2FC] = 0xFF;
    M[0x11] = 0xFF;
}

void Misja::title() {
    M[0x2FC] = 0xFF;                      // STX $02FC (X = $FF po TXS)
    M[0x22F] = 0;                         // ekran wyłączony
    waitKey();
    M.setWord(0x200, 0x6BB6);             // DLI ekranu tytułowego
    M.antic(0x0E, 0xC0);
    M.setWord(0x230, 0x6826);             // lista wyświetlania tytułu
    for (int x = 7; x >= 0; --x) M.gtia(x, 0);
    for (int x = 4; x >= 0; --x) M[0x2C4 + x] = M[0x6821 + x];
    clearPages();
    buildRoomTable();
    sound(0x80);
    if (M[0x6803] & 0x80) {
        M[0x6803]--;
        sound(0x0D);
        sound(0x01);
    }
    printPage(0);
    if (!(M[0x6EA5] & 0x80)) {
        // $694D – strona końca gry (6 + przyczyna)
        M[0x22F] = 0x3D;
        int a = M[0x6EA5];
        if (a == 0) {
            for (int x = 5; x >= 0; --x) M[0x6E0A + x] = ((M[0x6ED4 + x] & 7) + 0x30) & 0xFF;
        }
        bool c = titlePage(a + 6);
        if (M[0x6EA5] == 0) {
            do {                            // $6974: INC $AF65 / JSR $6B4C / BCC
                M[0xAF65]++;
                c = waitKey();
            } while (!c);
        }
        clearText();
        M[0x6EA5] = 0x80;
    }
    // $68E0 – strony tytułowe
    printRow(0x00, 0x00, 0x01);
    printRow(0x1F, 0x0F, 0x00);
    for (int y = 3; y >= 0; --y) {
        const int x = M[0x6EDD + y];
        M[0xAE00 + x] = 0x4A;
        M[0xAE01 + x] = 0x4B;
    }
    M[0x22F] = 0x3D;
    for (int x = 10; x > 0; --x) waitKey();
    for (int y = 1; y < 6; ++y) {
        if (titlePage(y)) {
            afterGame(playGame(false));     // $6928: CLC; JSR $7400 – gra
        }
    }
    playGame(true);                         // SEC; JSR $7400 – pokaz
    throw Restart{};
}

// $692C – po grze: przyczyna końca, wynik rekordowy, powrót do $6878
void Misja::afterGame(int reason) {
    M[0x6EA5] = reason;
    for (int x = 2; x >= 0; --x) {
        if (M[0xEF + x] < M[0xEC + x]) throw Restart{};
        if (M[0xEF + x] > M[0xEC + x]) {
            for (int k = 2; k >= 0; --k) M[0xEC + k] = M[0xEF + k];
            throw Restart{};
        }
    }
    throw Restart{};
}

// $6987 – strona tekstu: wypisanie, rozjaśnienie, pauza, ściemnienie. C = naciśnięto START/fire.
bool Misja::titlePage(int page) {
    clearText();
    printPage(page);
    M[0x84] = 0;
    do {
        if (waitKey()) return true;
        if (waitKey()) return true;
        for (int y = 7; y >= 0; --y) {
            int x = (M[0xAF70 + y] & 0xF0) | M[0xAF60 + y];
            if ((M[0xAF70 + y] & 0x0F) >= M[0x84]) x++;
            M[0xAF60 + y] = x & 0xFF;
        }
        M[0x84]++;
    } while (M[0x84] < 0x10);
    if (M[0x6EA5] == 0) return true;      // C = 1 po CMP #$10
    int y = 0;
    do {
        if (waitKey()) return true;
        y = (y - 1) & 0xFF;
    } while (y != 0);
    M[0x84] = 0x0F;
    do {
        if (waitKey()) return true;
        if (waitKey()) return true;
        for (int k = 7; k >= 0; --k) {
            const int a = M[0xAF60 + k];
            M[0xAF60 + k] = (a & 0x0F) ? (a - 1) & 0xFF : 0;
        }
        M[0x84]--;
    } while (!(M[0x84] & 0x80));
    return false;
}

// $69F5 – czyszczenie tekstu (zostawia znaki ramki >= $4E) i kolorów wierszy
void Misja::clearText() {
    int x = 0;
    do {
        int a = M[0xAE00 + x];
        if (a >= 0x4E) a = 0;
        M[0xAE00 + x] = a;
        if (x < 8) M[0xAF60 + x] = 0;
        x = (x - 1) & 0xFF;
    } while (x != 0);
}

// $6A10 – wiersz z tablicy $6EA6 (A = indeks, X = kolumna, Y = wiersz)
void Misja::printRow(int a, int x, int y) {
    M[0x83] = y;
    M[0x82] = x;
    y = a;
    x = textPos();
    for (;;) {
        a = M[0x6EA6 + y];
        if (a & 0x80) {
            M[0x83]++;
            a &= 0x7F;
            if (a == 0x7F) return;
            x = textPos();
        }
        a = (a << 1) & 0xFF;
        M[0xAE00 + x] = a;
        M[0xAE01 + x] = a | 1;
        x = (x + 2) & 0xFF;
        y = (y + 1) & 0xFF;
        if (y == 0) return;   // (BNE – w praktyce zawsze kończy znacznik $FF)
    }
}

// $6A39 – strona tekstu nr A: kolory wierszy, potem tekst
void Misja::printPage(int a) {
    int x = (a << 1) & 0xFF;
    int ptr = 0x6BDD;
    int y;
    for (;;) {
        M[0x83] = M[ptr];
        M[0x82] = M[ptr + 1];
        y = 2;
        if (x == 0) break;
        while (M[ptr + y] != 0x80) y++;
        y++;
        x--;
        ptr += y;
    }
    M.setWord(0x80, ptr);
    // kolory
    x = M[0x83];
    while (M[ptr + y] != 0x80) {
        M[0xAF70 + x] = M[ptr + y];
        if (x >= 8) M[0xAF60 + x] = M[ptr + y];
        x = (x + 1) & 0xFF;
        y++;
    }
    y++;
    // tekst
    x = textPos();
    for (;;) {
        a = M[ptr + y];
        if (a == 0) {
            y++;
            M[0x83]++;
            x = textPos();
            continue;
        }
        if (a & 0x80) {
            a &= 0x7F;
            if (a == 0) return;
            y++;
            const int d = (a - 0x30) & 0xFF;
            const int carry = d >> 7;
            M[0x85] = (d << 1) & 0xFF;
            x = (x + M[0x85] + carry) & 0xFF;
            continue;
        }
        M[0x85] = y;
        int k = 8;
        for (; k >= 0; --k) {
            if (a == M[0x6ED4 + k]) break;
        }
        if (k >= 0) {
            a = k;
            if (a >= 4) a += 0x1B;
            a += 0x3C;
        }
        if (a < 0x61) a += 0x20;
        if (a == 0x40) a = 0;
        a |= 0x80;
        y = M[0x85] + 1;
        if (M[0x84] & 0x80) M[0xAF00 + x] = a; else M[0xAE00 + x] = a;
        x = (x + 1) & 0xFF;
    }
}

// $6AD9 – X = wiersz × 32 + kolumna; $84 bit 7 = druga strona ($AF00)
int Misja::textPos() {
    const int v = M[0x83] << 5;
    M[0x84] = (v & 0x100) ? 0x80 : 0;
    return (v + M[0x82]) & 0xFF;
}

// $6AEC
void Misja::clearPages() {
    for (int x = 0; x < 256; ++x) {
        M[0xAE00 + x] = 0;
        M[0xAF00 + x] = 0;
    }
}

// $6AF9 – tablica adresów pomieszczeń $AB00/$AC00 (tylko przy pierwszym wywołaniu)
void Misja::buildRoomTable() {
    if (M[0x6AFA] != 0) return;
    M[0x6AFA]++;
    M[0x89] = 0;
    do {
        findRoom();
        M[0xAB00 + M[0x89]] = M[0x80];
        M[0xAC00 + M[0x89]] = M[0x81];
        M[0x89]++;
    } while (M[0x89] != 0);
}

// $6B17 – szukanie pomieszczenia $89 w danych od $1240 ([nr, długość, dane...])
void Misja::findRoom() {
    M.setWord(0x80, 0x1240);
    for (;;) {
        const int p = M.word(0x80);
        if (M[p + 1] == 0) { M[0x81] = 0; return; }
        if (M[p] == M[0x89]) { M.setWord(0x80, p + 2); return; }
        M.setWord(0x80, p + M[p + 1]);
    }
}

// $6B4C – czekanie na ramkę; C = START albo fire. Odczyt klawiatury i sekretne hasło.
bool Misja::waitKey() {
    M.waitFrame();
    M[0x4D] = 0;
    if (M[0x2FC] == 0xFF) {
        const int a = 0xFF & M.trig(0) & M.trig(1) & M.consol();
        return a == 0;
    }
    for (int x = 0; x < 5; ++x) M[0xAFA0 + x] = M[0xAFA1 + x];
    M[0xAFA5] = keyTable(M[0x2FC]);
    M[0x2FC] = 0xFF;
    if (M[0x6803] != 0) return false;
    for (int x = 5; x >= 0; --x) {
        if (M[0xAFA0 + x] != M[0x6E9F + x]) return false;
    }
    // „jasiek”
    M[0x6EA8] = 0x26;
    M[0x6803] = 0x80;
    M[0x2C8] = 0x0F;
    M.waitFrame();
    throw Restart{};
}

// $FB51 w ROM – kod klawisza -> ATASCII (małe litery)
int Misja::keyTable(int code) {
    static const char* keys[64] = {"l", "j", ";", "", "", "k", "+", "*", "o", "", "p", "u", "\x9b", "i", "-", "=",
                                   "v", "", "c", "", "", "b", "x", "z", "4", "", "3", "6", "\x1b", "5", "2", "1",
                                   ",", " ", ".", "n", "", "m", "/", "", "r", "", "e", "y", "\x7f", "t", "w", "q",
                                   "9", "", "0", "7", "\x7e", "8", "<", ">", "f", "h", "d", "", "", "g", "s", "a"};
    if (code < 0 || code >= 64) return 0;
    return static_cast<uint8_t>(keys[code][0]);
}

// $6BB6 – DLI ekranu tytułowego: zestaw znaków na zmianę $58/$5C, kolor wiersza w COLPF3
void Misja::titleDli() {
    if (M.vcount() < 8) M[0x00] = 0;
    const int x = M[0x00] >> 1;
    M.antic(9, (M[0x00] & 1) ? 0x5C : 0x58);
    M[0x00]++;
    M.gtia(0x19, M[0xAF60 + x]);
}
