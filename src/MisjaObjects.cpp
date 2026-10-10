//
//  MisjaObjects.cpp
//  Misja (C++ / SDL3)
//
//  Obiekty w pomieszczeniu ($8BF0-$94A3): 8 miejsc, każde z kodem ($98+X), klatką
//  i kierunkiem ($A0+X), kolumną ($A8+X) i wierszem w punktach ($B0+X). Kod wybiera
//  procedurę (tablica $94DB). Obiekty są rysowane znakami $E0-$EF składanymi z grafiki
//  ($4000), maski ($B200) i tła spod spodu (bufory $AD00/$AD80 -> znaki $5300/$5700).
//
//  $D7 bieżący obiekt   $8A numer znaku (2 × $D7)   $B8 bit 7 = bez rysowania (nadrabianie)
//  $9544 opóźnienia   $954C kafel źródłowy działka   $9554-$9594 zapamiętany stan pomieszczeń
//

#include "Misja.h"

// $91C8 (JSR $8BF0) – ruch wszystkich obiektów
void Misja::objects91C8() {
    if (!(M[0xB8] & 0x80)) objectHits();
    for (M[0xD7] = 0; M[0xD7] < 8; M[0xD7]++) {
        const int x = M[0xD7];
        const int a = M[0x98 + x] & 0x7F;
        for (int y = 0; y < 0x20; y += 3) {
            if (a != M[0x94DB + y]) continue;
            M[0x91ED] = M[0x94DC + y];
            M[0x91EE] = M[0x94DD + y];
            objectHandler(M.word(0x91ED));
            break;
        }
    }
}

void Misja::objectHandler(int addr) {
    switch (addr) {
    case 0x8C6F: obj8C6F(); break;
    case 0x8C58: break;
    case 0x8C5F: obj8C5F(); break;
    case 0x8C46: obj8C46(); break;
    case 0x8C59: obj8C59(); break;
    case 0x8CDC: obj8CDC(); break;
    case 0x8D83: obj8D83(); break;
    case 0x8D4D: obj8D4D(); break;
    case 0x8D62: obj8D62(); break;
    case 0x8DCB: obj8DCB(); break;
    case 0x8DD8: obj8DD8(); break;
    default: break;
    }
}

// $8C05 (JSR $8BFF) – nowy pocisk działka/robota: A ≠ 0 – w prawo; X kolumna, Y wiersz. C = brak miejsca
bool Misja::spawnObject(int a, int x, int y) {
    M[0x84] = x;
    M[0x85] = y;
    int k = 7;
    for (; k >= 0; --k) {
        if (M[0x98 + k] == 0x70) break;
    }
    if (k < 0) return true;
    M[0xA8 + k] = M[0x84];
    M[0xA0 + k] = a != 0 ? 4 : 0;
    M[0x98 + k] = 0x7C;
    M[0xB0 + k] = (M[0x85] + 0x10) & 0xFF;
    const ObjTile t = objTile(k);
    M[0xB0 + k] = M[0x85];
    if (t.a != 0) objTurn(k);
    sound(0x0F);
    return false;
}

// $8C46 – kod $3C
void Misja::obj8C46() {
    const ObjResult r = obj8E47();
    if (!r.c) return;
    objShot(r.a);
}

// $8C4B
void Misja::objShot(int a) {
    if (!(a & 0x80)) sound(6);
    M[0x98 + M[0xD7]] = 0x94;
}

// $8C59 – kod $3E
void Misja::obj8C59() {
    const ObjResult r = obj8E62();
    if (r.c) objShot(r.a);
}

// $8C5F – kod $14 (wybuch)
void Misja::obj8C5F() {
    const int x = objDraw();
    M[0xB0 + x]++;
    if ((M[0xB0 + x] & 7) == 0) M[0x98 + x] = 0x70;
}

// $8C6F – kod $7C (pocisk w locie)
void Misja::obj8C6F() {
    // zabezpieczenie: komórka $01 (suma kontrolna programu ładującego) musi być równa $0484
    M[0x80] = 0xFF;
    M[0x81] = 0xFF;
    M[0x87] = 4;
    M[0x86] = 0x82;
    if (M[0x0001] != M[0x0484]) {
        const int y = M.random();
        M[0xBA00 + y] = M[0xD6];
    }
    int x = objDraw();
    if ((M[0xA0 + x] & 3) == 3) {
        const int b = M[0xB0 + x];
        M[0xB0 + x] = (b + 0x20) & 0xFF;
        const ObjTile t = objTile(x);
        M[0xB0 + x] = (b + 0x10) & 0xFF;
        if (t.a != 0 || (M.random() & 3) == 0) {
            objTurn(x);
            return;
        }
    }
    objAnim(3, 1);
}

// $8CC4 – zmiana kierunku pocisku
void Misja::objTurn(int x) {
    const int y = M[0xA0 + x] >= 4 ? 0x3E : 0x3C;
    M[0x98 + x] = y;
    M[0xA0 + x] = y == 0x3C ? 8 : 0x0C;
}

// $8CDC – kod $73 (miejsce na nowego żołnierza)
void Misja::obj8CDC() {
    if (M[0xB8] & 0x80) return;
    int x = M[0xD7];
    if (M[0xB0 + x] != 0) {
        M[0xB0 + x]--;
        return;
    }
    if (M[0x9544 + x] != 0) {
        M[0x9544 + x]--;
        return;
    }
    for (;;) {
        // $8CF1
        int a = ((M.random() & 0x17) + 0x10) & 0xFF;
        M[0xA0 + x] = a;
        int y;
        if (a >= 0x20) { y = 0x1F; a = 0xFE; }
        else { y = 0x1E; a = 0x20; }
        M[0xA8 + x] = a;
        M[0x98 + x] = y;
        do { a = M.random() & 0x70; } while (a >= 0x70);
        M[0xB0 + x] = a;
        for (;;) {
            M[0xB0 + x] = (M[0xB0 + x] + 0x10) & 0xFF;
            if (M[0xB0 + x] & 0x80) { obj8D77(x); return; }
            if (objTile(x).a != 0) break;
        }
        M[0xB0 + x] = (M[0xB0 + x] - 0x10) & 0xFF;
        if (objTile(x).c) continue;
        M[0xB0 + x] = (M[0xB0 + x] - 0x10) & 0xFF;
        if (objTile(x).c) continue;
        for (int k = 7; k >= 0; --k) M[0x9544 + k] = 0x15;
        return;
    }
}

// $8D4D – kod $1E (żołnierz idzie w lewo)
void Misja::obj8D4D() {
    soldierShoot(0);
    const ObjResult r = soldierMove(0xFF);
    if (!r.c) return;
    const int x = M[0xD7];
    if (r.a & 0x80) { obj8D77(x); return; }
    M[0xA0 + x] = 0x20;
    M[0x98 + x] = 0x1F;
}

// $8D62 – kod $1F (żołnierz idzie w prawo)
void Misja::obj8D62() {
    soldierShoot(2);
    const ObjResult r = soldierMove(1);
    if (!r.c) return;
    const int x = M[0xD7];
    if (r.a & 0x80) { obj8D77(x); return; }
    M[0xA0 + x] = 0x10;
    M[0x98 + x] = 0x1E;
}

// $8D77 – żołnierz zniknął
void Misja::obj8D77(int x) {
    M[0x98 + x] = 0x73;
    M[0xB0 + x] = M.random() | 0x1F;
}

// $8D83 – kod $6F (trafiony żołnierz)
void Misja::obj8D83() {
    const int x = objDrawTall();
    M[0xB0 + x]++;
    if (M[0xB0 + x] & 7) return;
    M[0x98 + x] = 0x73;
    M[0xB0 + x] = 0xF0;
}

// $8D97 – żołnierz strzela (odłamek rodzaju 6 albo 2)
void Misja::soldierShoot(int a) {
    if (M[0xB8] & 0x80) return;
    int x = M[0xD7];
    if (M[0xA8 + x] >= 0x20) return;
    if (M[0xA0 + x] & 7) return;
    if (M.random() & 0x0F) return;
    const int y = ((M[0xB0 + x] >> 1) + 9) & 0xFF;
    const int px = ((a + M[0xA8 + x]) << 2) & 0xFF;
    spawnDebris(a != 0 ? a : 6, px, y);
}

// $8DCB – kod $6C (działko patrzy w lewo)
void Misja::obj8DCB() {
    gunFire();
    if (gunTurn8F03()) M[0x98 + M[0xD7]] = 0x72;
}

// $8DD8 – kod $72 (działko patrzy w prawo)
void Misja::obj8DD8() {
    gunFire();
    if (gunTurn8F5F()) M[0x98 + M[0xD7]] = 0x6C;
}

// $8DE5 – działko: strzał albo obrót do gracza
void Misja::gunFire() {
    if (M[0xB8] & 0x80) return;
    if (M.random() & 7) return;
    int x = M[0xD7];
    const int a = M[0xA0 + x];
    if (a >= 0x40) {
        M[0x84] = a & 3;
        if (M.random() & 7) return;
        int y = 0x30;
        const int p = (M[0xA8 + x] << 2) & 0xFF;
        if ((p & 0x80) || p < M[0xB9]) y = 0x38;
        M[0xA0 + x] = y | M[0x84];
        return;
    }
    const int dir = a >= 0x38 ? 1 : 0;
    if ((a & 3) != 2) return;
    const int y = M[0xB0 + x] & 0xF0;
    const int col = (M[0xA8 + x] + 1) & 0xFF;
    if (spawnObject(dir, col, y)) return;
    x = M[0xD7];
    M[0xA0 + x] = (M[0xA0 + x] & 3) | 0x40;
    sound(5);
}

// $8E47 – pocisk w lewo
Misja::ObjResult Misja::obj8E47() {
    const int x = objDraw();
    if (!(M[0xA0 + x] & 1)) return {objAnim(3, 1), false};
    M[0xA8 + x]--;
    const ObjTile t = objTile(x);
    if (!t.c) return {objAnim(3, 1), false};
    M[0xA8 + x]++;
    return {t.a, true};
}

// $8E62 – pocisk w prawo
Misja::ObjResult Misja::obj8E62() {
    const int x = objDraw();
    if (!(M[0xA0 + x] & 1)) return {objAnim(3, 1), false};
    M[0xA8 + x] += 2;
    const ObjTile t = objTile(x);
    M[0xA8 + x]--;
    if (!t.c) return {objAnim(3, 1), false};
    M[0xA8 + x]--;
    return {t.a, true};
}

// $8E7A / $8ECB – krok żołnierza (dir $FF w lewo, 1 w prawo). C = koniec drogi
Misja::ObjResult Misja::soldierMove(int dir) {
    // $8EDF – co drugi krok tylko rysowanie, animacja nóg
    const int x = objDrawTall();
    M[0x84] = 0x8E;                      // PLA / STA $84 – starszy bajt adresu powrotu ($8E7C / $8ECD)
    M[0xB0 + x]++;
    if (M[0xB0 + x] & 1) return {0, false};
    M[0xB0 + x] &= 0xF0;
    if ((M[0xA0 + x] & 3) != 3) {
        objAnim(7, 1);
        return {0, false};
    }
    const int savedY = M[0xB0 + x], savedX = M[0xA8 + x];
    M[0x85] = dir;
    if (dir == 0xFF) M[0xA8 + x]--; else M[0xA8 + x] += 2;
    auto blocked = [&](int a) {
        M[0xA8 + x] = savedX;
        M[0xB0 + x] = savedY;
        return ObjResult{a, true};
    };
    ObjTile t = objTile(x);
    if (t.c) return blocked(t.a);
    M[0xB0 + x] = (M[0xB0 + x] + 0x10) & 0xFF;
    t = objTile(x);
    if (t.c) return blocked(t.a);
    const int code = t.y & 0x3F;
    if (code == 0x2B) return blocked(code);
    M[0xB0 + x] = (M[0xB0 + x] + 0x10 + (code >= 0x2B ? 1 : 0)) & 0xFF;
    t = objTile(x);
    if (t.a == 0) return blocked(0);
    if ((t.y < 0 ? code : t.y) == 0x2A) return blocked(t.a);
    M[0xA8 + x] = (savedX + M[0x85]) & 0xFF;
    M[0xB0 + x] = savedY;
    objAnim(7, 1);
    return {0, false};
}

// $8F03 – działko w lewo: obrót, gdy pod nim kończy się podłoga. C = obrót
bool Misja::gunTurn8F03() {
    const int x = objDrawWide();
    M[0xB0 + x]++;
    if (M[0xB0 + x] & 1) return false;
    M[0xB0 + x] &= 0xF0;
    if ((M[0xA0 + x] & 3) != 3) {
        objAnim(3, 1);
        return false;
    }
    const int savedX = M[0xA8 + x], savedY = M[0xB0 + x];
    M[0xA8 + x]--;
    return gunStep(x, savedX, savedY, 0xFF);
}

// $8F5F – działko w prawo
bool Misja::gunTurn8F5F() {
    const int x = objDrawWide();
    M[0xB0 + x]++;
    if (M[0xB0 + x] & 1) return false;
    M[0xB0 + x] &= 0xF0;
    if ((M[0xA0 + x] & 3) != 0) {
        objAnim(3, 0xFF);
        return false;
    }
    const int savedX = M[0xA8 + x];
    M[0xA8 + x] = (M[0xA8 + x] + 4) & 0xFF;
    const int savedY = M[0xB0 + x];
    M[0x86] = 1;
    return gunStep(x, savedX, savedY, 1);
}

// $8F26
bool Misja::gunStep(int x, int savedX, int savedY, int dir) {
    M[0x85] = dir;
    auto turn = [&] {
        M[0xB0 + x] = savedY;
        M[0xA8 + x] = savedX;
        return true;
    };
    const ObjTile t1 = objTile(x);
    if (t1.c) return turn();
    M[0xB0 + x] = (M[0xB0 + x] + 0x10) & 0xFF;
    const ObjTile t = objTile(x);
    if (t.a == 0) return turn();
    if ((t.y < 0 ? t1.y : t.y) == 0x2A) return turn();
    M[0xB0 + x] = savedY;
    M[0xA8 + x] = (savedX + M[0x85]) & 0xFF;
    if (M[0x85] & 0x80) objAnim(3, 1); else objAnim(3, 0xFF);
    return false;
}

// $8F89 – kafel pod obiektem X: a = właściwość, c = a >= 3, y = kod.
// Poza ekranem: a = $80, c = 1, y = -1 (rejestr Y bez zmian).
Misja::ObjTile Misja::objTile(int x) {
    int a = M[0xA8 + x];
    if (a & 0x80) {
        if (a < 0xFE) return {0x80, true, -1};
        a = 0;
    }
    if (a >= 0x20) {
        if (a >= 0x22) return {0x80, true, -1};
        a = 0x1E;
    }
    M[0x84] = a >> 1;
    a = M[0xB0 + x];
    if (a >= 0xC0) {
        if (a < 0xF0) return {0x80, true, -1};
        a = 0;
    }
    if (a >= 0x80) {
        if (a >= 0x90) return {0x80, true, -1};
        a = 0x70;
    }
    const int code = M[0xBA00 + (a | M[0x84])] & 0x3F;
    const int p = M[0x1200 + code] & 0x0F;
    return {p, p >= 3, code};
}

// $8FCD – rysowanie obiektu (2 znaki w poziomie); zwraca X = $D7
int Misja::objDraw() {
    if (M[0xB8] & 0x80) return M[0xD7];
    const int x = M[0xD7];
    M[0x8A] = (x << 1) & 0xFF;
    const int row = M[0xB0 + x];
    const int col = M[0xA8 + x];
    drawObjectChar(col, row);
    M[0x8A]++;
    drawObjectChar((col + 1) & 0xFF, row);
    return M[0xD7];
}

// $8FE9 – obiekt wysoki (2 × 2 znaki)
int Misja::objDrawTall() {
    if (M[0xB8] & 0x80) return M[0xD7];
    objDraw();
    const int x = M[0xD7];
    M[0x8A]++;
    const int s = M[0xA0 + x] + 8;
    M[0xA0 + x] = s & 0xFF;
    const int row = (M[0xB0 + x] + 0x10 + (s > 0xFF ? 1 : 0)) & 0xFF;
    const int col = M[0xA8 + x];
    drawObjectChar(col, row);
    M[0x8A]++;
    drawObjectChar((col + 1) & 0xFF, row);
    M[0xA0 + x] = (M[0xA0 + x] - 8) & 0xFF;
    return M[0xD7];
}

// $9015 – obiekt szeroki (4 znaki w poziomie)
int Misja::objDrawWide() {
    if (M[0xB8] & 0x80) return M[0xD7];
    objDraw();
    const int x = M[0xD7];
    M[0x8A]++;
    M[0xA0 + x] = (M[0xA0 + x] + 4) & 0xFF;
    const int row = M[0xB0 + x];
    const int col = (M[0xA8 + x] + 2) & 0xFF;
    drawObjectChar(col, row);
    M[0x8A]++;
    drawObjectChar((col + 1) & 0xFF, row);
    M[0xA0 + x] = (M[0xA0 + x] - 4) & 0xFF;
    return M[0xD7];
}

// $9040 / $9044 – kolejna klatka: bity `mask` w $A0+$D7 zwiększone o dir (1 / -1); zwraca A
int Misja::objAnim(int mask, int dir) {
    M[0x82] = dir == 1 ? 1 : 0xFF;
    M[0x84] = mask;
    M[0x85] = mask ^ 0xFF;
    const int x = M[0xD7];
    int y = M[0xA0 + x];
    M[0xA0 + x] = y & M[0x85];
    y = (y + (M[0x82] & 0x80 ? -1 : 1)) & 0xFF;
    M[0xA0 + x] = (y & M[0x84]) | M[0xA0 + x];
    return M[0xA0 + x];
}

// $9074 – znak obiektu ($E0 + $8A) w kolumnie X, wierszu Y (w punktach)
void Misja::drawObjectChar(int col, int row) {
    if (row & 0x80) return;
    if (col >= 0x20) return;
    M[0x82] = (((row & 0xF0) << 1) & 0xFF) | (col & 0x1F);
    int x = M[0xD7];
    objectGraphics(M[0x94FC + M[0xA0 + x]]);
    int y = M[0x82];
    const int screen = M.word(0x94);
    const int under = M[(screen + y) & 0xFFFF];
    M[(screen + y) & 0xFFFF] = (0xE0 + M[0x8A]) & 0xFF;
    M[0x83] = 4;
    const int a = under & 0x7F;
    if (a >= 0x60 && a < 0x70) {
        // pod spodem inny obiekt – jego znak z bufora $AD00
        M[0x86] = ((a & 0x0F) << 3) & 0xFF;
        M[0x87] = 0xAD;
        M[0x83] = 0;
    } else {
        charGraphics(a);
    }
    M[0x84] = M[0x80];
    M[0x85] = (M[0x81] + 0x72) & 0xFF;
    const int top = ((M[0x8A] << 3) + 7) & 0xFF;
    x = top;
    const int code = M[0x98 + M[0xD7]];
    auto obj = [&](int yy) { return M[(M.word(0x80) + yy) & 0xFFFF]; };
    auto msk = [&](int yy) { return M[(M.word(0x84) + yy) & 0xFFFF]; };
    auto bg = [&](int yy) { return M[(M.word(0x86) + yy) & 0xFFFF]; };
    if (code & 0x80) {
        // obiekt znika: grafika przerzedzana losowo
        for (y = 7; y >= 0; --y) {
            M[0xAD00 + x] = obj(y) & M.random();
            M[0xAD00 + x] |= bg(y) & msk(y);
            x = (x - 1) & 0xFF;
        }
        nextCharHalf(false);
        x = top;
        for (y = 7; y >= 0; --y) {
            M[0xAD80 + x] = obj(y) & M.random();
            M[0xAD80 + x] |= bg(y) & msk(y);
            x = (x - 1) & 0xFF;
        }
        return;
    }
    if (M[0xA0 + M[0xD7]] >= 8) {
        for (y = 7; y >= 0; --y) {
            M[0xAD00 + x] = (bg(y) & msk(y)) | obj(y);
            x = (x - 1) & 0xFF;
        }
        nextCharHalf(false);
        x = top;
        for (y = 7; y >= 0; --y) {
            M[0xAD80 + x] = (bg(y) & msk(y)) | obj(y);
            x = (x - 1) & 0xFF;
        }
        return;
    }
    // $913D – klatki 0-7: tło z dolną połową przesuniętą (wysuwanie z podłogi)
    for (y = 7; y >= 0; --y) {
        M[0xAD00 + x] = bg(y);
        x = (x - 1) & 0xFF;
    }
    nextCharHalf(true);
    x = top;
    for (y = 7; y >= 0; --y) {
        M[0xAD80 + x] = bg(y);
        x = (x - 1) & 0xFF;
    }
    y = M[0xA0 + M[0xD7]] & 3;
    x = (x + M[0x9178 + y]) & 0xFF;
    for (y = 3; y >= 0; --y) {
        M[0xAD00 + x] = (M[0xAD00 + x] & msk(y)) | obj(y);
        x = (x - 1) & 0xFF;
    }
}

// $917C / $9189 – adresy drugiej połowy znaku (zestaw $54/$58 jest 4 strony dalej)
void Misja::nextCharHalf(bool only9189) {
    int c = 0;
    if (!only9189) {
        int r = M[0x81] + 4;
        c = r > 0xFF;
        M[0x81] = r & 0xFF;
        r = M[0x85] + 4 + c;
        c = r > 0xFF;
        M[0x85] = r & 0xFF;
    }
    int r = M[0x87] + M[0x83] + c;
    c = r > 0xFF;
    M[0x87] = r & 0xFF;
    if (M[0x83] == 0) M[0x86] = (M[0x86] + 0x80 + c) & 0xFF;
}

// $919A (JSR $8BFC) – adres grafiki znaku A w zestawie $5000 -> $86/$87
void Misja::charGraphics(int a) {
    const int v = (a & 0xFF) << 3;
    M[0x86] = v & 0xFF;
    M[0x87] = ((v >> 8) + 0x50) & 0xFF;
}

// $91B0 – grafika obiektu: $80/$81 = $4000 + (2 × A + bit 0 z $8A) × 8
void Misja::objectGraphics(int a) {
    const int v = ((a << 1) | (M[0x8A] & 1)) & 0xFF;
    M[0x86] = v;
    charGraphics(v);
    M[0x80] = M[0x86];
    M[0x81] = (M[0x87] + 0xF0) & 0xFF;
}

// $9202 (JSR $8BF9) – bufory znaków obiektów do zestawów znaków
void Misja::objects9202() {
    for (int x = 0x7F; x >= 0; --x) {
        M[0x5300 + x] = M[0xAD00 + x];
        M[0x5700 + x] = M[0xAD80 + x];
    }
}

// $9214 (JSR $8BF3) – maski obiektów $B200-$B9FF z grafiki $4000-$47FF
void Misja::objects9214() {
    M[0x86] = 0;
    M[0x80] = 0;
    M[0x81] = 0xB2;
    M[0x87] = 0x40;
    for (int x = 7; x >= 0; --x) {
        for (int y = 0; y < 256; ++y) {
            int a = M[M.word(0x86) + y];
            M[0x82] = a;
            a = ((a << 1) | M[0x82]) & 0xAA;
            M[0x82] = a;
            a = ((a >> 1) | M[0x82]) ^ 0xFF;
            M[M.word(0x80) + y] = a & 0xFF;
        }
        M[0x81]++;
        M[0x87]++;
        M[0x9555 + x] = 0;
        M[0x9575 + x] = 0;
    }
}

// $924B (JSR $8BF6) – obiekty nowego pomieszczenia
void Misja::objects924B() {
    for (int x = 7; x >= 0; --x) {
        M[0x954C + x] = 0;
        M[0x98 + x] = 0x80;
        M[0xB0 + x] = (M.random() & 0x0F) | 0x10;
    }
    int a = M[0x6F00 + M[0x89]];
    if (a == 0x20) return;
    int y = (((a - 0x30) & 0xFF) << 2) & 0xFF;
    for (int x = 0; x < 8; x += 2) {
        a = M[0x94B3 + y];
        M[0x98 + x] = a;
        if (a == 0x70) M[0x99 + x] = 0x70;
        y++;
    }
    // działka w miejscu kafli $70/$80 zamieniają wolne miejsca $74
    M[0x84] = 0;
    for (int x = 0; x < 0x80; ++x) {
        const int p = M[0x1200 + (M[0xBA00 + x] & 0x3F)] & 0xF0;
        int code;
        if (p == 0x70) code = 0x6C;
        else if (p == 0x80) code = 0x72;
        else continue;
        int k = M[0x84];
        for (; k < 8; k += 2) {
            if (M[0x98 + k] == 0x74) break;
        }
        if (k >= 8) break;
        M[0x954C + k] = x;
        M[0x98 + k] = code;
        M[0xA0 + k] = code == 0x6C ? 0x30 : 0x38;
        M[0x84] += 2;
        M[0xBA00 + x] = M[0xBA01 + x];
        M[0xA8 + k] = (x & 0x0F) << 1;
        M[0xB0 + k] = x & 0xF0;
    }
    // powrót do niedawno opuszczonego pomieszczenia: stan obiektów i nadrobienie czasu
    if (M[0x9555] == 0) return;
    if (M[0x89] != M[0x9554]) return;
    int frames = M[0x97];
    if (frames & 0x80) return;
    for (int x = 0x1F; x >= 0; --x) M[0x98 + x] = M[0x9555 + x];
    if (M[0xCB] == 0x0C || M[0xCB] == 0x0D) frames = 0x0A;
    if (frames >= 0x18) frames = 0x18;
    M[0xB8] = 0x80;
    for (int k = frames; k >= 0; --k) objects91C8();
    M[0xB8] = (M[0xB8] << 1) & 0xFF;
}

// $9325 (JSR $8C02) – opuszczenie pomieszczenia: działka z powrotem w mapę, zapamiętanie stanu
void Misja::objects9325() {
    for (int x = 7; x >= 0; --x) {
        const int y = M[0x98 + x];
        if (y == 0x6C || y == 0x72) M[0xBA00 + M[0x954C + x]] = 0x30;
    }
    M[0x9554] = M[0x96];
    for (int x = 0x1F; x >= 0; --x) {
        M[0x9555 + x] = M[0x9575 + x];
        M[0x9575 + x] = M[0x98 + x];
    }
}

// $9352 – trafienia obiektów (tablica $94A4: kody, nowy kod, prostokąt, procedura)
void Misja::objectHits() {
    for (int x = 7; x >= 0; --x) {
        M[0x8A] = x;
        for (int y = 0; !(M[0x94A4 + y] & 0x80); y += 7) {
            const int c = M[0x98 + x];
            if (c != M[0x94A4 + y] && c != M[0x94A5 + y]) continue;
            M[0x86] = M[0x94A6 + y];
            M[0x82] = M[0x94A7 + y];
            M[0x83] = M[0x94A8 + y];
            M[0x938A] = M[0x94A9 + y];
            M[0x938B] = M[0x94AA + y];
            switch (M.word(0x938A)) {
            case 0x9392: hits9392(); break;
            case 0x93AF: hits93AF(); break;
            default: break;
            }
            break;
        }
        x = M[0x8A];
    }
}

// $9392 – pocisk działka (kody $3C/$3E)
void Misja::hits9392() {
    M[0x8B] = 0;
    M[0x8C] = 1;
    if (!grenadeHits()) scoreTimes(0x12);
    if (!bulletHits()) scoreTimes(0x15);
}

// $93AF – żołnierz (kody $1E/$1F)
void Misja::hits93AF() {
    M[0x8B] = 0;
    M[0x8C] = 5;
    if (!grenadeHits()) scoreTimes(0x50);
    if (!bulletHits()) scoreTimes(0x08);
}

// $93C9 – punkty A tyle razy, ile trafień ($87)
void Misja::scoreTimes(int a) {
    int x = M[0x87];
    do {
        addScore(a);
        x = (x - 1) & 0xFF;
    } while (x != 0);
}

// $93D4 – trafienia pociskami; C (true) = brak
bool Misja::bulletHits() {
    M[0x87] = 0;
    for (int x = 3; x >= 0; --x) {
        if (M[0xC4 + x] == 0) continue;
        const int y = (M[0xC0 + x] - 0x20) & 0xFF;
        int px = M[0xBC + x];
        if (!hitTest(px, y)) {
            M[0x87]++;
            M[0xBC + x] = 0xBF;
        }
    }
    return M[0x87] == 0;
}

// $9404 – trafienie granatem; C (true) = brak
bool Misja::grenadeHits() {
    M[0x87] = 0;
    int y = (M[0xC9] - 0x20) & 0xFF;
    int x = M[0xC8];
    const int d0 = M[0xD0];
    if (d0 == 0) return M[0x87] == 0;
    if (d0 < 3) {
        if (!hitTest(x, y)) {
            M[0xD0] = 3;
            M[0x87]++;
        }
        return M[0x87] == 0;
    }
    y = (y - 2) & 0xFF;
    if (!hitTest(x, y)) M[0x87]++;
    if (M[0xD2] < 2 || M[0xD2] >= 4) return M[0x87] == 0;
    x = (x - 1) & 0xFF;
    if (!hitTest(x, y)) M[0x87]++;
    x = (x + 3) & 0xFF;
    if (!hitTest(x, y)) M[0x87]++;
    return M[0x87] == 0;
}

// $9451 – czy punkt (X, Y) leży w prostokącie obiektu $8A; false (C = 0) = trafiony.
// X po powrocie jak na 6502 (przy chybieniu w pionie i przy trafieniu: górna krawędź).
bool Misja::hitTest(int& px, int py) {
    const int k = M[0x8A];
    int a = (M[0xA8 + k] << 2) & 0xFF;
    if (a & 0x80) a = 0;
    M[0x80] = (a + M[0x8B]) & 0xFF;
    M[0x81] = ((M[0xB0 + k] & 0xF0) + M[0x8C]) & 0xFF;
    if (px < M[0x80]) return true;
    M[0x80] = (M[0x80] + M[0x82]) & 0xFF;
    if (px >= M[0x80]) return true;
    px = M[0x81];
    if (py < M[0x81]) return true;
    M[0x81] = (M[0x81] + M[0x83]) & 0xFF;
    if (py >= M[0x81]) return true;
    M[0x98 + M[0x8A]] = M[0x86];
    sound(6);
    return false;
}
