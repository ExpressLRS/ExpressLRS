#include <functional>
#include <stack>

#include "targets.h"

#define STATE_IGNORED -1

#define EVENT_IMMEDIATE -1 // immediately do this after the entry-function
#define EVENT_TIMEOUT -2

#define ACTION_GOTO 0
#define ACTION_NEXT 1
#define ACTION_PREVIOUS 2
#define ACTION_PUSH 3
#define ACTION_POP 4

typedef int8_t fsm_state_t;
typedef int8_t fsm_event_t;
typedef int8_t fsm_action_t;

typedef struct fsm_state_event_s
{
    fsm_event_t event;
    fsm_action_t action;
    fsm_event_t next;
} fsm_state_event_t;

typedef struct fsm_state_entry_s
{
    fsm_state_t state;
    std::function<void(bool init)> entry;
    uint16_t timeout;
    fsm_state_event_t const *events;
    uint8_t event_count;
} fsm_state_entry_t;

class FiniteStateMachine
{
public:
    explicit FiniteStateMachine(fsm_state_entry_t const *fsm) : fsm(fsm) {}
    void start(uint32_t now, fsm_state_t state);
    void handleEvent(uint32_t now, fsm_event_t event);
    fsm_state_t getParentState() { return fsm[state_index_stack.top()].state; }
    void popState() { force_pop = true; }

private:
    const fsm_state_entry_t *fsm;

    static std::stack<int> state_index_stack;
    static int last_state_index;
    static int current_state_index;
    static uint32_t current_state_entered;
    static bool force_pop;
};
