#include "../../include/enjin2/input/input_state.hpp"
#include <cstring>

namespace enjin2 {

void input_advance_frame(InputState* state) {
    state->prev_buttons = state->buttons;
    memcpy(state->prev_axes, state->axes, sizeof(state->axes));
    state->buttons = 0;
    memset(state->axes, 0, sizeof(state->axes));
}

} // namespace enjin2
