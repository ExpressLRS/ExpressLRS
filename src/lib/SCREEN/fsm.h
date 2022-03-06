#include <functional>
#include <stack>

#include "targets.h"

#define STATE_IGNORED -1

#define ACTION_GOTO 0
#define ACTION_NEXT 1
#define ACTION_PREVIOUS 2
#define ACTION_PUSH 3
#define ACTION_POP 4

#define EVENT_IMMEDIATE -1 // immediately do this after the entry-function
#define EVENT_TIMEOUT -2

typedef struct fsm_state_event_s
{
    int8_t event;
    int8_t action;
    int8_t next;
} fsm_state_event_t;

typedef struct fsm_state_entry_s
{
    int8_t state;
    std::function<void()> entry;
    uint32_t timeout;
    fsm_state_event_t const *events;
    uint8_t event_count;
} fsm_state_entry_t;

class FiniteStateMachine
{
public:
    FiniteStateMachine(fsm_state_entry_t const *fsm) : fsm(fsm) {}
    void start(uint32_t now);
    void handleEvent(uint32_t now, int8_t event);
    uint8_t getParentState() { return state_index_stack.top(); }
    void popState() { force_pop = true; }

private:
    const fsm_state_entry_t *fsm;

    static std::stack<int> state_index_stack;
    static int current_state_index;
    static uint32_t current_state_entered;
    static bool force_pop;
};
