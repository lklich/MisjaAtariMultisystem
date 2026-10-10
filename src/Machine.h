//
//  Machine.h
//  Misja (C++ / SDL3)
//
//  „Sprzęt” Atari, na którym działa przepisany kod Misji: pamięć RAM, ANTIC (lista
//  wyświetlania, tryby znakowe i graficzne), GTIA (gracze i pociski, priorytety, kolizje),
//  rejestry POKEY oraz to, co gra bierze z systemu operacyjnego Atari (przerwanie VBI:
//  zegar RTCLOK, liczniki CDTMV, rejestry-cienie kolorów, klawiatura, BREAK).
//
//  Kod gry nie jest emulowany – to zwykły C++ (Misja*.cpp). Tu jest tylko to, co na Atari
//  robi sprzęt i ROM. Wszystko liczone w liniach obrazu PAL: 312 linii na ramkę, 50 Hz.
//
//  Kod gry działa jak na Atari jako jeden ciąg instrukcji z oczekiwaniem na sprzęt
//  (np. na zmianę zegara albo na linię obrazu). Dlatego wykonuje się w osobnym wątku,
//  który przekazuje sterowanie tam i z powrotem – w danej chwili pracuje zawsze tylko
//  jedna strona, więc przebieg jest w pełni powtarzalny.
//

#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

class Machine {
public:
    static constexpr int width = 336;    // ekran: takty koloru 44-211 (po 2 piksele)
    static constexpr int height = 240;   // linie 8-247

    Machine();
    ~Machine();

    // MARK: Pamięć
    std::vector<uint8_t> m = std::vector<uint8_t>(0x10000, 0);
    uint8_t& operator[](int addr) { return m[addr & 0xFFFF]; }
    int word(int addr) const { return m[addr & 0xFFFF] | (m[(addr + 1) & 0xFFFF] << 8); }
    void setWord(int addr, int v) { m[addr & 0xFFFF] = v & 0xFF; m[(addr + 1) & 0xFFFF] = (v >> 8) & 0xFF; }

    // MARK: Rejestry sprzętu (zapis)
    void gtia(int reg, int v);         // $D000-$D01F
    void antic(int reg, int v);        // $D400-$D40F
    void pokey(int reg, int v);        // $D200-$D20F
    void hitclr();                     // $D01E
    void pia(int reg, int v);          // $D300-$D303

    // MARK: Rejestry sprzętu (odczyt)
    int collision(int reg) const { return coll_[reg & 15]; }   // $D000-$D00F
    int trig(int n) const;             // $D010-$D013 (0 = wciśnięty)
    int consol() const;                // $D01F (bity 0 START, 1 SELECT, 2 OPTION; 0 = wciśnięty)
    int vcount() const { return line_ >> 1; }   // $D40B
    int random();                      // $D20A
    int kbcode() const { return key_ < 0 ? 0xFF : key_; }   // $D209
    int serin() const { return serin_; }                     // $D20D
    void setSerin(int v) { serin_ = v & 0xFF; }
    int piaRead(int reg) const;        // $D300-$D303

    // MARK: Wejście (z warstwy okna)
    void setStick(int bits) { stick_ = bits & 0x0F; }   // 0 = kierunek wciśnięty
    void setTrig(bool pressed) { trigger_ = pressed; }
    void setConsol(int bits) { consol_ = bits & 7; }     // 0 = przycisk wciśnięty
    /// Naciśnięcie klawisza (KBCODE z bitami SHIFT $40 i CONTROL $80); -1 = puszczony.
    void keyDown(int code);
    void keyUp();
    void pressBreak();

    // MARK: Przebieg czasu
    /// Jedna ramka PAL (312 linii): przerwania, kod gry, obraz.
    void frame();
    const std::vector<uint32_t>& pixels() const { return pixels_; }
    std::vector<uint32_t>& pixels() { return pixels_; }
    int gtiaReg(int reg) const { return gtia_[reg & 0x1F]; }
    long long frameCount() const { return frames_; }
    int line() const { return line_; }

    /// Kod gry (wątek) – oczekiwanie, aż warunek będzie spełniony (sprawdzany co linię).
    void waitUntil(const std::function<bool()>& cond);
    /// Czekanie na zmianę RTCLOK ($14) – czyli na koniec przerwania VBI.
    void waitFrame();
    /// Czekanie na VCOUNT == v.
    void waitVcount(int v);

    // Połączenia z kodem gry
    std::function<void()> program;          // kod startowy wątku gry
    std::function<void()> dli;              // procedura przerwania DLI (wektor $200)
    std::function<void()> deferredVBI;      // VVBLKD (procedura dźwięku)
    /// Zapis AUDF/AUDC: (audf, audc | kanał << 8) – dla syntezy dźwięku.
    std::function<void(int, int)> onPokey;

    /// Uruchomienie kodu gry od początku (np. RESET). `entry` zastępuje `program` jednorazowo.
    void restart(std::function<void()> entry = nullptr);
    /// Wątek gry stoi w miejscu oczekiwania oznaczonym jako bezpieczne (zapis stanu).
    bool parkedAt(int tag) const { return parkedTag_ == tag; }
    void setParkTag(int tag) { parkTag_ = tag; }

    // Stan sprzętu do zapisów gry
    struct HwState {
        std::array<uint8_t, 32> gtia{};
        std::array<uint8_t, 16> antic{};
        std::array<uint8_t, 16> coll{};
        uint32_t rnd = 0;
        std::array<uint8_t, 4> pia{};
    };
    HwState hwState() const;
    void setHwState(const HwState& s);

    /// Paleta Atari PAL (0xAARRGGBB).
    static const std::vector<uint32_t>& palette();

private:
    // rejestry
    std::array<uint8_t, 32> gtia_{};
    std::array<uint8_t, 16> antic_{};
    std::array<uint8_t, 16> coll_{};
    uint8_t audf_[4] = {0, 0, 0, 0}, audc_[4] = {0, 0, 0, 0};
    std::array<uint8_t, 4> pia_{};     // PORTA (wyjście), DDRA, PACTL, PBCTL
    int serin_ = 0xFF;
    // wejście
    int stick_ = 0x0F;
    bool trigger_ = false;
    int consol_ = 7;
    int key_ = -1;
    uint32_t rnd_ = 0x1FFFF;
    // czas
    int line_ = 0;
    long long frames_ = 0;
    // ANTIC
    int dlpc_ = 0;
    int memscan_ = 0;
    int lineMemscan_ = 0;
    int mode_ = 0;
    int modeLeft_ = 0;
    int modeLine_ = 0;
    bool modeDLI_ = false;
    bool dlWait_ = false;
    bool inDli_ = false;
    std::vector<std::pair<int, int>> pending_;
    void anticLine();
    void renderLine();
    void vbi();
    std::vector<uint32_t> pixels_;

    // wątek gry
    std::thread thread_;
    std::mutex lock_;
    std::condition_variable cv_;
    bool logicTurn_ = false;      // true: pracuje wątek gry
    bool stop_ = false;           // żądanie zakończenia wątku
    bool started_ = false;
    std::function<bool()> cond_;  // warunek, na który czeka wątek gry
    std::function<void()> entry_;
    int parkTag_ = 0;             // znacznik bieżącego oczekiwania (ustawia kod gry)
    int parkedTag_ = 0;
    void startThread();
    void stopThread();
    void runLogic();              // oddaje sterowanie wątkowi gry, gdy jego warunek jest spełniony
    void threadMain();
};

/// Rzucane w wątku gry, gdy trzeba go zatrzymać (RESET, wczytanie zapisu, koniec programu).
struct MachineStop {};
