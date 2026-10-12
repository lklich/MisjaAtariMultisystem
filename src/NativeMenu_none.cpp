//
//  NativeMenu_none.cpp
//  Misja (C++ / SDL3)
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/
//  Systemy bez menu systemowego (Linux): wszystko jest w menu w grze pod F1.

#include "NativeMenu.h"

namespace NativeMenu {

void install(SDL_Window*, Uint32) {}
void update(const State&) {}
void setFullscreen(bool) {}
bool available() { return false; }

}  // namespace NativeMenu
