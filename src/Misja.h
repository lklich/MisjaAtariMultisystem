//
//  Misja.h
//  Misja (C++ / SDL3)
//
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/

#pragma once

#include "Machine.h"

#include <atomic>
#include <cstdint>
#include <vector>

/// Skok do $6878 (start programu) z dowolnego miejsca.
struct Restart {};
/// CONTROL+SHIFT+ESC – na Atari JMP $E474 (gorący start systemu).
struct WarmStart {};

class Misja {
public:
    /// Obraz pamięci po wczytaniu kasety ($0000-$FFFF; ważne: $0480-$05FF i $1200-$9FFF).
    Misja(Machine& machine, const std::vector<uint8_t>& image);

    /// Ustawia pamięć tak jak po wczytaniu kasety przez program startowy.
    static void loadImage(Machine& m, const std::vector<uint8_t>& image);

    void start();                 // $6878
    void resume(int point);       // wznowienie zapisu (główna pętla: 1 – $765D, 2 – $7672)
    void restartState();          // stan po RESET (dźwięk, VBI, klawiatura)

    // Ułatwienia (nie ma ich w oryginale) – ustawiane z menu
    std::atomic<bool> infiniteEnergy{false};
    std::atomic<bool> infiniteAmmo{false};
    std::atomic<bool> infiniteGrenades{false};
    void dli();                   // wektor $0200
    void deferredVBI();           // wektor $0224

private:
    Machine& M;
    bool c_ = false;              // przeniesienie 6502 tam, gdzie przechodzi między procedurami

    struct Tile { int a = 0; int x = 0; int y = 0; };          // $8412: A, X, Y
    struct ObjTile { int a = 0; bool c = false; int y = 0; };  // $8F89: A, C, Y
    struct ObjResult { int a = 0; bool c = false; };

    // Odczyt przez wskaźnik – jak procesor (rejestry sprzętu pod $D000-$D7FF)
    int read(int addr);

    // MisjaTitle.cpp
    void title();
    void afterGame(int reason);
    bool titlePage(int page);
    void clearText();
    void printRow(int a, int x, int y);
    void printPage(int a);
    int textPos();
    void clearPages();
    void buildRoomTable();
    void findRoom();
    bool waitKey();
    static int keyTable(int code);
    void titleDli();

    // MisjaSound.cpp
    void sound(int a);
    int soundStart();
    void soundVBI();
    bool soundStep(int x);

    // MisjaGame.cpp
    int playGame(bool demo);
    int mainLoop(int entry);
    void applyCheats();
    void roomExit();
    void enterRoom();
    void playerReset();
    void unpackRoom();
    bool roomPtr();
    void copyRoomToScreen();
    void swapScreens();
    void roomCharsets();
    void animateChars();
    void statusPanel();
    void damage(int a);
    void addScore(int a);
    bool pickUp(int a);
    void refillEnergy();
    void printNumber(int a, int& y);
    void printBCD(int a, int& y);
    void printDigit(int a, int& y);
    bool checkEnd(int& y);
    void victory();
    void helicopterFrame();
    void timeOut();
    void gameDli();

    // MisjaPlayer.cpp
    void player7F6C();
    void playerState();
    void state0();
    void state1();
    void state3();
    void state4();
    void state5();
    void state6();
    void state7();
    void state8();
    void state9();
    void state10();
    void state11();
    void throwGrenade();
    int onGround();
    int headroom();
    int wallAt(int x);
    int ladder();
    bool topOfLadder();
    Tile tileAt(int px, int py);
    bool fall();
    bool jumpUp();
    bool walk();
    void readDirection();
    int jumpStep();
    void player851F();
    void playerDraw();
    int drawFrame(int a, int x);
    void framePtr(int a);
    int stick();
    int fire();

    // MisjaShots.cpp
    void debris();
    int debrisCheck();
    void debrisMove();
    void debrisDraw();
    void spawnDebris(int a, int x, int y);
    bool enemies88B2();
    void grenade();
    bool grenadeStep();
    void grenadeErase();
    void grenadeDraw();
    void explosion();
    void explodeTile(int px, int py);
    void bulletsMove();
    void shots8A54();
    void bulletKill();
    void bulletsErase();
    void bulletsDraw();
    void enemies8AE8();

    // MisjaObjects.cpp
    void objects91C8();
    void objectHandler(int addr);
    bool spawnObject(int a, int x, int y);
    void obj8C46();
    void objShot(int a);
    void obj8C59();
    void obj8C5F();
    void obj8C6F();
    void objTurn(int x);
    void obj8CDC();
    void obj8D4D();
    void obj8D62();
    void obj8D77(int x);
    void obj8D83();
    void soldierShoot(int a);
    void obj8DCB();
    void obj8DD8();
    void gunFire();
    ObjResult obj8E47();
    ObjResult obj8E62();
    ObjResult soldierMove(int dir);
    bool gunTurn8F03();
    bool gunTurn8F5F();
    bool gunStep(int x, int savedX, int savedY, int dir);
    ObjTile objTile(int x);
    int objDraw();
    int objDrawTall();
    int objDrawWide();
    int objAnim(int mask, int dir);
    void drawObjectChar(int col, int row);
    void nextCharHalf(bool only9189);
    void charGraphics(int a);
    void objectGraphics(int a);
    void objects9202();
    void objects9214();
    void objects924B();
    void objects9325();
    void objectHits();
    void hits9392();
    void hits93AF();
    void scoreTimes(int a);
    bool bulletHits();
    bool grenadeHits();
    bool hitTest(int& px, int py);

    // MisjaRooms.cpp
    void rooms969F();
    void rooms96B2();
    void lift96C6();
    void liftEnd();
    void liftPlayerY();
    void switch9771();
    void switch978A();
    bool playerTouches(int x);
    void specialSetup();
    void rooms98A7();
    void liftCharCopy(int a, int x);
    void liftGraphics(int a);
    void lift9925();
    void door996A();
    void fillDown(int a, int x);
    bool shaftStep(int a, int& x, int dir);
    void tileAnimations();
    void tile9A1F(int x);
    void tile9A6C(int carry);
    void tile9A87(int carry);
    void tile9A9B(int carry);
    bool tileFrame7(int& a, bool& c);
    void tileSpawn(int x, int y, int carry);
    void tile9B0C(int x);
    void tile9B52();
    void tileDebris(int kind, int dx);
    void tile9B9B();
    void tile9BA9(int x);
    void liftChars();
    void liftCharsCopy(int a, int x);
    void tile9BF1(int x, int carry);
    void tile9C3F();
    void setTile(int x, int a);
    void drawTileAt(int a, int y);
    void roomSwitchesInit();
    void rooms9D21();
    void rooms9D89();
};
