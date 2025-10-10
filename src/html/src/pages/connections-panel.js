import {html, LitElement} from "lit";
import {customElement} from "lit/decorators.js";
import '../assets/mui.js';

@customElement('connections-panel')
class ConnectionsPanel extends LitElement {
    createRenderRoot() {
        return this;
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">PWM Pin Functions</div>
            <div class="mui-panel">
                Set PWM output mode and failsafe positions.
                <ul>
                    <li><b>Output:</b> Receiver output pin</li>
                    <li><b>Features:</b> If an output is capable of supporting another function, that is indicated
                        here
                    </li>
                    <li><b>Mode:</b> Output frequency, 10KHz 0-100% duty cycle, binary On/Off, DShot, Serial, or I2C
                        (some options are pin dependant)
                    </li>
                    <ul>
                        <li>When enabling serial pins, be sure to select the <b>Serial Protocol</b> below and <b>UART
                            baud</b> on the <b>Options</b> tab
                        </li>
                    </ul>
                    <li><b>Input:</b> Input channel from the handset</li>
                    <li><b>Invert:</b> Invert input channel position</li>
                    <li><b>750us:</b> Use half pulse width (494-1006us) with center 750us instead of 988-2012us</li>
                    <li><b>Failsafe</b>
                        <ul>
                            <li>"Set Position" sets the servo to an absolute "Failsafe Pos"
                                <ul>
                                    <li>Does not use "Invert" flag</li>
                                    <li>Value will be halved if "750us" flag is set</li>
                                    <li>Will be converted to binary for "On/Off" mode (>1500us = HIGH)</li>
                                </ul>
                            </li>
                            <li>"No Pulses" stops sending pulses
                                <ul>
                                    <li>Unpowers servos</li>
                                    <li>May disarm ESCs</li>
                                </ul>
                            </li>
                            <li>"Last Position" continues sending last received channel position</li>
                        </ul>
                    </li>
                </ul>
                <form action="/pwm" id="pwm" method="POST">
                    
                </form>
            </div>
        `;
    }
}
