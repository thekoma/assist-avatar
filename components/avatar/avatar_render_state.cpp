// esphome.h FIRST: it auto-includes every component header (alphabetically),
// including avatar_module.h, so esphome::Color + avatar::ColorSet are fully
// declared before avatar_render_state.h (and the dispatch/module headers it
// pulls) are parsed. Including the render-state header first instead would let
// avatar_module.h's own `#include "esphome.h"` re-enter the avatar headers while
// ColorSet is still mid-declaration → "ColorSet does not name a type". Every
// other ESPHome .cpp leads with esphome.h for the same reason.
#include "esphome.h"
#include "avatar_render_state.h"

namespace avatar {
// ODR definition — one translation unit owns the table.
StateEntry g_avatar_states[AVATAR_NUM_PHASES];
}  // namespace avatar
