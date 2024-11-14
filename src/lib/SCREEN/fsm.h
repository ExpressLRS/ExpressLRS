#include <deque>

#include "targets.h"

#define STATE_LAST -1

typedef int8_t fsm_state_t;

enum fsm_event_t : int8_t {
    EVENT_TIMEOUT = -2,
    EVENT_IMMEDIATE = -1, // immediately do this after the entry-function

    EVENT_ENTER = 0,
    EVENT_UP,
    EVENT_DOWN,
    EVENT_LEFT,
    EVENT_RIGHT,
    EVENT_LONG_ENTER,
    EVENT_LONG_UP,
    EVENT_LONG_DOWN,
    EVENT_LONG_LEFT,
    EVENT_LONG_RIGHT
};

enum fsm_action_t : int8_t {
    ACTION_GOTO,
    ACTION_NEXT,
    ACTION_PREVIOUS,
    ACTION_PUSH,
    ACTION_POP,
    ACTION_POPALL
};

typedef struct fsm_state_entry_s fsm_state_entry_t;

typedef struct fsm_state_event_s
{
    fsm_event_t event;
    fsm_action_t action;
    union {
        const fsm_state_entry_t *fsm;
        fsm_state_t state;
    } next;
} fsm_state_event_t;

#define GOTO(x)    ACTION_GOTO, {.state = x}
#define PUSH(x)    ACTION_PUSH, {.fsm = x}

typedef struct fsm_state_entry_s
{
    fsm_state_t state;
    bool (*available)();
    void (*entry)(bool init);
    uint16_t timeout;
    fsm_state_event_t const *events;
    uint8_t event_count;
} fsm_state_entry_t;

class FiniteStateMachine
{
public:
    explicit FiniteStateMachine(fsm_state_entry_t const *fsm);
    void start(uint32_t now, fsm_state_t state);
    void handleEvent(uint32_t now, fsm_event_t event);
    fsm_state_t getCurrentState();
    fsm_state_t getParentState();
    void popState() { force_pop = true; }

    void jumpTo(fsm_state_entry_t const *fsm, fsm_state_t state);

private:
    const fsm_state_entry_t *root_fsm;

    typedef struct fsm_pos_s {
        const fsm_state_entry_t *fsm;
        int index;
    } fsm_pos_t;

    const fsm_state_entry_t *current_fsm;   // the currently running FSM
    int current_index;                      // index into the current FSM
    uint32_t current_state_entered;         // the time (ms) when the current state was entered

    std::deque<fsm_pos_t> fsm_stack;

    bool force_pop;
};
