#include "fsm.h"

std::stack<int> FiniteStateMachine::state_index_stack;
int FiniteStateMachine::last_state_index;
int FiniteStateMachine::current_state_index;
uint32_t FiniteStateMachine::current_state_entered = 0;
bool FiniteStateMachine::force_pop = false;

void FiniteStateMachine::start(uint32_t now, fsm_state_t state)
{
    state_index_stack.push(STATE_IGNORED);
    current_state_index = state;
    fsm[current_state_index].entry(current_state_index != last_state_index);
    last_state_index = current_state_index;
    current_state_entered = now;
}

void FiniteStateMachine::handleEvent(uint32_t now, fsm_event_t event)
{
    if (force_pop)
    {
        force_pop = false;
        current_state_index = state_index_stack.top();
        state_index_stack.pop();
        fsm[current_state_index].entry(current_state_index != last_state_index);
        last_state_index = current_state_index;
        current_state_entered = now;
        return handleEvent(now, EVENT_IMMEDIATE);
    }
    // Event timeout has not occurred
    if (event == EVENT_TIMEOUT && now - current_state_entered < fsm[current_state_index].timeout)
    {
        return;
    }
    // scan FSM for matching event in current state
    for (int index = 0 ; index < fsm[current_state_index].event_count ; index++)
    {
        const fsm_state_event_t &action = fsm[current_state_index].events[index];
        if (event == action.event)
        {
            switch (action.action)
            {
                case ACTION_PUSH:
                    state_index_stack.push(current_state_index);
                    // FALL-THROUGH
                case ACTION_GOTO:
                    for (int i = 0 ; fsm[i].state != STATE_IGNORED ; i++)
                    {
                        if (fsm[i].state == action.next)
                        {
                            current_state_index = i;
                            break;
                        }
                    }
                    break;
                case ACTION_POP:
                    current_state_index = state_index_stack.top();
                    state_index_stack.pop();
                    break;
                case ACTION_NEXT:
                    current_state_index++;
                    break;
                case ACTION_PREVIOUS:
                    current_state_index--;
                    break;
            }
            fsm[current_state_index].entry(current_state_index != last_state_index);
            last_state_index = current_state_index;
            current_state_entered = now;
            return handleEvent(now, EVENT_IMMEDIATE);
        }
    }
}
