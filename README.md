# Misja – wersja C++ / SDL3 (macOS, Windows, Linux)

Gra **Misja** (Atari XL/XE, LK Avalon 1990, autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior)
przepisana ręcznie z kodu 6502 na **C++17 i SDL3**. Ten sam kod kompiluje się na macOS, Windows i Linuksie.

Kryptonim akcji: Misja. Zadanie: zniszczenie podziemnej bazy wojskowej – umieścić 4 pojemniki z uranem 233
w centrum bazy i uciec śmigłowcem.

## Jak to działa?

Każda procedura oryginału ma swój odpowiednik w C++ (adres 6502 w komentarzu). Zmienne gry leżą pod tymi
samymi adresami co na Atari, dlatego przebieg gry da się porównać z oryginałem bajt w bajt
(zob. „Zgodność z oryginałem”).

> **Program nie zawiera żadnych danych oryginalnej gry** – grafiki, pomieszczeń, dźwięków ani tekstów.
> Przy pierwszym uruchomieniu trzeba wskazać obraz kasety oryginalnej Misji (`.cas`).
> Dane są sprawdzane sumą SHA-256 i zapisywane w katalogu użytkownika; potem plik kasety nie jest potrzebny.

---

## Co jest potrzebne do kompilacji

| System | Narzędzia |
|---|---|
| macOS 11+ | Xcode lub Command Line Tools (`xcode-select --install`), **CMake 3.20+** (`brew install cmake` albo CMake.app z cmake.org), git |
| Windows 10/11 | **Visual Studio 2022** z zestawem „Programowanie aplikacji klasycznych w języku C++” (ma CMake i git), albo MSYS2/MinGW-w64 |
| Linux | `g++` lub `clang`, CMake, git, nagłówki X11/Wayland i ALSA/PulseAudio (lista niżej) |

**SDL3 nie trzeba instalować.** Przy pierwszej konfiguracji CMake sam pobiera SDL3 z GitHuba
(wersja `release-3.4.18`) i linkuje go statycznie, więc powstaje jeden plik programu bez dodatkowych
bibliotek. Pierwsze budowanie trwa przez to 1–3 minuty i wymaga internetu. Później wszystko działa
już bez sieci.

---

## macOS

```bash
cd robboSDL
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
open build/Misja.app
```

Wynik: **`build/Misja.app`** – zwykła aplikacja z ikoną, którą można skopiować do `/Applications`.

Wersja uniwersalna (Apple Silicon i Intel w jednym pliku):

```bash
cmake -S . -B build-universal -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal -j
```

Projekt Xcode (jeśli wolisz pracować w Xcode):

```bash
cmake -S . -B build-xcode -G Xcode
open build-xcode/Misja.xcodeproj
```

Aplikacja nie jest podpisana. Jeśli przeniesiesz ją na inny Mac, przy pierwszym uruchomieniu kliknij ją
prawym przyciskiem → **Otwórz** → **Otwórz**.

---

## Windows (Visual Studio 2022)

W **Developer PowerShell for VS 2022** (albo zwykłym wierszu poleceń, jeśli CMake jest w PATH):

```powershell
cd robboSDL
cmake -S . -B build
cmake --build build --config Release
.\build\Release\Misja.exe
```

Wynik: **`build\Release\Misja.exe`** – jeden plik z ikoną. Działa bez instalowania SDL i bez
Visual C++ Redistributable (biblioteka C jest linkowana statycznie), więc można go po prostu skopiować
na inny komputer.

Można też w Visual Studio wybrać **Plik → Otwórz → Folder…**, wskazać `robboSDL`, poczekać na
konfigurację CMake, wybrać cel **Misja.exe** i nacisnąć **F5**.

### Windows – MinGW-w64 (MSYS2)

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja git
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Misja.exe
```

---

## Linux (np. Ubuntu/Debian)

```bash
sudo apt install build-essential cmake git ninja-build \
     libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxfixes-dev \
     libwayland-dev libxkbcommon-dev libegl-dev libgl-dev libasound2-dev libpulse-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Misja
```

---

## SDL3 zainstalowane w systemie (opcjonalnie)

Jeśli nie chcesz, żeby CMake pobierał SDL3 (np. budowanie bez internetu), zainstaluj SDL3 w wersji
3.4 lub nowszej i włącz opcję `MISJA_SYSTEM_SDL`:

```bash
# macOS
brew install sdl3
cmake -S . -B build -DMISJA_SYSTEM_SDL=ON

# Windows (vcpkg)
vcpkg install sdl3
cmake -S . -B build -DMISJA_SYSTEM_SDL=ON -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
```

Inną wersję SDL z GitHuba można wybrać opcją `-DMISJA_SDL_TAG=release-3.x.y`.

---

## Pierwsze uruchomienie

1. Pojawi się ekran „Potrzebne są dane z oryginalnej kasety”.
2. Przeciągnij na okno plik **`Misja.cas`** albo naciśnij **Enter** i go wskaż.
   W Windows można też przeciągnąć plik na ikonę `Misja.exe`.
3. Program sprawdza sumy kontrolne wszystkich 284 bloków taśmy i sumę SHA-256 całości – od razu
   pojawia się ekran tytułowy.

### Gdzie są dane

| System | Katalog |
|---|---|
| macOS | `~/Library/Application Support/Misja/` |
| Windows | `%APPDATA%\Misja\` |
| Linux | `~/.local/share/Misja/` |

W katalogu są: `misja_mem.bin` (dane odczytane z kasety) i `saves.bin` (zapisy gry).

---

## Sterowanie

Gra była sterowana joystickiem; klawiatura Atari służy do pauzy i przerwania gry.

| Klawisz | Działanie |
|---|---|
| Strzałki | ruch (joystick) |
| Spacja | strzał (przycisk joysticka) |
| Shift (lewy lub prawy) | rzut granatem (w oryginale: joystick w dół + przycisk) |
| strzałka w górę | skok, wspinanie po drabinie, jazda windą |
| Enter (albo F4) | START – rozpoczęcie gry |
| F3 / F2 | SELECT / OPTION |
| P | pauza (drugie naciśnięcie – dalej); w oryginale była to spacja |
| Esc | przerwanie gry (klawisz ESC Atari) |
| Pause albo F7 | BREAK – przerwanie gry |
| Backspace | RESET – powrót do ekranu tytułowego |
| litery na ekranie tytułowym | sekretne hasło autora, jak w oryginale |

**Gamepad** (Xbox, PlayStation, Switch Pro i inne obsługiwane przez SDL): krzyżak lub lewa gałka – joystick,
przyciski A/B/X/Y – strzał, Start – START, Back/Select – pauza, Guide/PS – menu.

### Menu „Gra” – zapis stanu gry

- **macOS** – menu na pasku u góry ekranu (obok „Misja”),
- **Windows** – pasek menu w oknie (w trybie pełnoekranowym jest chowany),
- **Linux** – tylko menu w grze (F1).

**F1** (albo F10) otwiera menu rysowane w grze; gra jest wtedy wstrzymana. Skróty działają zawsze
(na macOS z klawiszem **Cmd**, w Windows i Linuksie z **Ctrl**):

| Skrót | Działanie |
|---|---|
| Cmd/Ctrl+S | zapisz stan gry (tylko w trakcie gry, nie w pokazie) |
| Cmd/Ctrl+L | lista zapisów: wczytywanie i usuwanie, z miniaturą ekranu, pomieszczeniem, wynikiem i energią |
| Cmd/Ctrl+R | RESET – ekran tytułowy |
| Cmd/Ctrl+O | wczytaj dane z kasety ponownie |
| Cmd/Ctrl+D | wyłącz/włącz dźwięk |
| F11 lub Alt+Enter | pełny ekran |
| Cmd/Ctrl+Q | zakończ |

Zapis zapamiętuje cały stan gry; po wczytaniu gra toczy się dalej dokładnie tak samo, jakby nie było przerwy.

---

## Budowa projektu

```
robboSDL/
├── CMakeLists.txt          budowanie (SDL3 pobierane automatycznie)
├── res/                    ikony (misja.icns, misja.ico), zasoby Windows, Info.plist dla macOS
└── src/
    ├── Misja.h, Misja.cpp  klasa gry: wczytanie pamięci jak program startowy z kasety, przerwania
    ├── MisjaTitle.cpp      $6878-$6BDC  ekran tytułowy, strony tekstu, hasło, strony końca gry
    ├── MisjaSound.cpp      $7000-$7152  efekty dźwiękowe (4 kanały POKEY, przerwanie VBI)
    ├── MisjaGame.cpp       $7400-$7F5F  główna pętla, pomieszczenia, panel stanu, energia, czas, zakończenie
    ├── MisjaPlayer.cpp     $7F60-$86EF  gracz: chodzenie, skoki, spadanie, drabiny, granaty
    ├── MisjaShots.cpp      $86F0-$8BEF  granat, pociski, odłamki
    ├── MisjaObjects.cpp    $8BF0-$968F  żołnierze, działka, pociski działek, trafienia
    ├── MisjaRooms.cpp      $9690-$9FFF  windy, szyby, przełączniki, drzwi, zapadnie, pojemniki z uranem
    ├── Machine.*           „sprzęt” Atari: pamięć, ANTIC (lista wyświetlania), GTIA (gracze, pociski,
    │                       priorytety, kolizje), PIA, rejestry POKEY, to co gra bierze z ROM (VBI,
    │                       zegary CDTMV, rejestry-cienie, klawiatura, BREAK)
    ├── Game.*              gra dla warstwy okna: wejście, dźwięk, zapis i wczytanie stanu
    ├── Pokey.*             synteza dźwięku POKEY
    ├── Palette.inc         paleta Atari PAL
    ├── OriginalTape.*      odczyt kasety .cas (blok startowy, 284 bloki z narastającym XOR) i weryfikacja
    ├── Sha256.*            SHA-256
    ├── SaveStore.*         zapisy gry z miniaturą
    ├── Storage.*           pliki i katalog danych (przez SDL, ścieżki UTF-8)
    ├── App.*               okno, dźwięk, klawiatura/gamepad, menu w grze, import kasety
    ├── NativeMenu*         systemowe menu „Gra”: _mac.mm (Cocoa), _win.cpp (Win32), _none.cpp (Linux)
    └── main.cpp            start programu i tryby testowe
```

Wszystko poza `App.*`, `Storage.*`, `NativeMenu*` i `main.cpp` to czysty C++ bez zależności od SDL.

Kod 6502 Misji czeka na sprzęt (na koniec ramki, na linię obrazu, na licznik CDTMV3). Żeby przepisany kod
miał tę samą budowę co oryginał, działa w osobnym wątku, który przekazuje sterowanie „sprzętowi” w tych
miejscach – w danej chwili pracuje zawsze tylko jedna strona, więc przebieg jest w pełni powtarzalny.
Kod gry wykonuje się w chwili, w której na Atari zaczynał pracę (np. w linii 164 obrazu), więc gra nigdy
nie zwalnia, nawet gdy na Atari jedna tura trwałaby dłużej niż dwie ramki.

### Zabezpieczenia oryginału

Misja sprawdza w kilku miejscach, czy została wczytana z oryginalnej kasety (m.in. ostatni bajt odebrany
przez POKEY, sumę kontrolną programu startowego w komórce $01, flagi bloku startowego, kopię bufora kasety,
kierunek portu PIA). Jeśli coś się nie zgadza, gra po cichu psuje mapę albo grafikę. Wszystkie te miejsca
są przepisane wiernie (część z nich zużywa liczby losowe, więc wpływa na przebieg gry), a `Machine`
odtwarza stan pamięci i sprzętu taki, jaki zostawia oryginalny program ładujący.

---

## Zgodność z oryginałem

Port był sprawdzany ramka po ramce z oryginalnym kodem 6502 uruchomionym w emulatorze Atari (poza
projektem): ta sama taśma, te same naciśnięcia, porównanie całej pamięci RAM po każdej ramce (poza stosem).
Sprawdzone przebiegi – wszystkie identyczne bajt w bajt:

- ekran tytułowy, wszystkie strony tekstu i pokaz (4000 ramek),
- gra sterowana automatem: chodzenie, skoki, drabiny, strzały, granaty, śmierć, kolejne gry,
- przejście przez wszystkie pomieszczenia bazy (ok. 33 600 ramek),
- osobne scenariusze: hasło, pauza, ESC, BREAK, koniec czasu, odkładanie pojemników z uranem,
  windy i przełączniki, bezczynność postaci, ucieczka śmigłowcem i strona końcowa.

Tryb `--trace` zapisuje skrót pamięci po każdej ramce, żeby można było powtórzyć takie porównanie.

---

## Tryby testowe (wiersz poleceń)

```bash
# N ramek bez okna i zapis ekranu do PNG; skrypt naciśnięć: "ramka:klawisz:1|0,..."
# klawisze: up down left right fire grenade start select option space esc break reset (space = pauza)
Misja --headless 900 gra.png "300:start:1,305:start:0,500:right:1,600:right:0" --tape Misja.cas

# jak wyżej, ale zamiast obrazu: skrót FNV-1a pamięci RAM po każdej ramce (4 bajty na ramkę)
Misja --trace 5000 slad.bin "300:start:1,305:start:0" --tape Misja.cas

# automatyczne zrzuty ekranów interfejsu (tytuł, menu, gra, zapis, wczytanie, RESET) i wyjście
Misja --shots katalog
```

Na macOS program w paczce uruchamia się z wiersza poleceń jako `build/Misja.app/Contents/MacOS/Misja`.
