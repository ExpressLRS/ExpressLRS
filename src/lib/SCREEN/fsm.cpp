#include "fsm.h"

FiniteStateMachine::FiniteStateMachine(fsm_state_entry_t const *fsm) : root_fsm(fsm)
{
    current_fsm = root_fsm;
    current_index = 0;
    current_state_entered = 0;
    force_pop = false;
}

fsm_state_t FiniteStateMachine::getCurrentState()
{
    return current_fsm[current_index].state;
}

fsm_state_t FiniteStateMachine::getParentState()
{
    if (fsm_stack.empty())
    {
        return STATE_LAST;
    }
    const fsm_pos_t pos = fsm_stack.back();
    return (pos.fsm)[pos.index].state;
}

void FiniteStateMachine::jumpTo(fsm_state_entry_t const *fsm, fsm_state_t state)
{
    fsm_stack.push_back({current_fsm, current_index});
    current_fsm = fsm;
    for (int i = 0 ; current_fsm[i].state != STATE_LAST ; i++)
    {
        if (current_fsm[i].state == state)
        {
            current_index = i;
            uint32_t now = millis();
            current_fsm[current_index].entry(true);
            current_state_entered = now;
            handleEvent(now, EVENT_IMMEDIATE);
            break;
        }
    }
}

void FiniteStateMachine::start(uint32_t now, fsm_state_t state)
{
    current_fsm = root_fsm;
    current_index = 0;
    current_state_entered = now;
    while (current_fsm[current_index].state != STATE_LAST)
    {
        if (current_fsm[current_index].state == state)
        {
            current_fsm[current_index].entry(true);
            break;
        }
        current_index++;
    }
}

void FiniteStateMachine::handleEvent(uint32_t now, fsm_event_t event)
{
    if (force_pop)
    {
        force_pop = false;

        const fsm_pos_t pos = fsm_stack.back();
        fsm_stack.pop_back();
        current_index = pos.index;
        current_fsm = pos.fsm;

        current_fsm[current_index].entry(true);
        current_state_entered = now;
        return handleEvent(now, EVENT_IMMEDIATE);
    }
    // Event timeout has not occurred
    if (event == EVENT_TIMEOUT && (now - current_state_entered < current_fsm[current_index].timeout || current_fsm[current_index].timeout == FSM_NO_TIMEOUT))
    {
        return;
    }
    // scan state for matching event in current state
    for (int index = 0 ; index < current_fsm[current_index].event_count ; index++)
    {
        const fsm_state_event_t &action = current_fsm[current_index].events[index];
        if (event == action.event)
        {
            bool init = true;
            switch (action.action)
            {
                case ACTION_PUSH:
                    fsm_stack.push_back({current_fsm, current_index});
                    current_fsm = action.next.fsm;
                    current_index = 0;
                    break;
                case ACTION_GOTO:
                    if (current_fsm[current_index].state == action.next.state)
                    {
                        init = false;
                    }
                    else
                    {
                        for (int i = 0 ; current_fsm[i].state != STATE_LAST ; i++)
                        {
                            if (current_fsm[i].state == action.next.state)
                            {
                                current_index = i;
                                break;
                            }
                        }
                    }
                    break;
                case ACTION_POP:
                    {
                        fsm_pos_t pos = fsm_stack.back();
                        current_index = pos.index;
                        current_fsm = pos.fsm;
                        fsm_stack.pop_back();
                    }
                    break;
                case ACTION_POPALL:
                    {
                        fsm_pos_t pos = fsm_stack.front();
                        current_index = pos.index;
                        current_fsm = pos.fsm;
                        fsm_stack.clear();
                    }
                    break;
                case ACTION_NEXT:
                    do {
                        current_index++;
                        if (current_fsm[current_index].state == STATE_LAST)
                        {
                            current_index = 0;
                        }
                    } while (current_fsm[current_index].available && !current_fsm[current_index].available());
                    break;
                case ACTION_PREVIOUS:
                    do {
                        if (current_index == 0)
                        {
                            while (current_fsm[current_index].state != STATE_LAST)
                            {
                                current_index++;
                            }
                        }
                        current_index--;
                    } while (current_fsm[current_index].available && !current_fsm[current_index].available());
                    break;
            }
            current_fsm[current_index].entry(init);
            current_state_entered = now;
            return handleEvent(now, EVENT_IMMEDIATE);
        }
    }
}
