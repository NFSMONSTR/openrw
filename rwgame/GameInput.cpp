#include "GameInput.hpp"
#include <unordered_map>

std::unordered_multimap<int, GameInputState::Control> kControls;
void GameInput::updateGameInputState(GameInputState *state,
                                     const SDL_Event &event) {
    switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            auto sym = event.key.keysym.sym;
            auto level = event.type == SDL_KEYDOWN ? 1.f : 0.f;
            auto& levels = state->levels;

            auto range = kControls.equal_range(sym);
            for (auto it = range.first; it != range.second; ++it) {
                levels[it->second] = level;
            }
        } break;
    }
}
