const HARDWARE_SCHEMA = [
    {
        title: 'CRSF Serial Pins', rows: [
            {
                id: 'serial_rx',
                label: 'RX pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Pin used to receive CRSF signal from the handset'
            },
            {
                id: 'serial_tx',
                label: 'TX pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Pin used to transmit CRSF telemetry to the handset (may be the same as the RX PIN)'
            },
        ]
    },

    /* FEATURE: NOT IS_8285 */
    {
        title: 'Serial2 Pins', rows: [
            {
                id: 'serial1_rx',
                label: 'RX pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Serial2 RX - ESP32 targets only'
            },
            {
                id: 'serial1_tx',
                label: 'TX pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Serial2 TX - ESP32 targets only'
            },
        ]
    },
    /* /FEATURE: NOT IS_8285 */

    {
        title: 'Radio Chip Pins & Options', rows: [
            /* FEATURE: NOT IS_SX127X */
            {
                id: 'radio_busy',
                label: 'BUSY pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'GPIO Input connected to SX128x busy pin'
            },
            /* /FEATURE: NOT IS_SX127X */
            /* FEATURE: IS_SX127X */
            {
                id: 'radio_dio0',
                label: 'DIO0 pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Interrupt pin for SX127x'
            },
            /* /FEATURE: IS_SX127X */
            /* FEATURE: NOT IS_SX127X */
            {
                id: 'radio_dio1',
                label: 'DIO1 pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Interrupt pin for SX128x/LR1121'
            },
            /* /FEATURE: NOT IS_SX127X */
            {
                id: 'radio_miso',
                label: 'MISO pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'MISO connected to (possibly) multiple SX1280/127x'
            },
            {
                id: 'radio_mosi',
                label: 'MOSI pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'MOSI connected to (possibly) multiple SX1280/127x'
            },
            {
                id: 'radio_nss',
                label: 'NSS pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Chip select pin for first SX1280/127x'
            },
            {
                id: 'radio_rst',
                label: 'RST pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Reset pin connected to (possibly) multiple SX1280/127x'
            },
            {
                id: 'radio_sck',
                label: 'SCK pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Clock pin connected to (possibly) multiple SX1280/127x'
            },
            /* FEATURE: NOT IS_SX127X */
            {
                id: 'radio_busy_2',
                label: 'BUSY_2 pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Busy pin for second SX1280'
            },
            /* /FEATURE: NOT IS_SX127X */
            /* FEATURE: IS_SX127X */
            {
                id: 'radio_dio0_2',
                label: 'DIO0_2 pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Interrupt pin for second SX127x'
            },
            /* /FEATURE: IS_SX127X */
            /* FEATURE: NOT IS_SX127X */
            {
                id: 'radio_dio1_2',
                label: 'DIO1_2 pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Interrupt pin for second SX1280'
            },
            /* /FEATURE: NOT IS_SX127X */
            {
                id: 'radio_nss_2',
                label: 'NSS_2 pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Chip select pin for second SX1280'
            },
            {
                id: 'radio_rst_2',
                label: 'RST_2 pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Reset pin connected to second SX1280/127x'
            },
            /* FEATURE: NOT IS_SX127X */
            {
                id: 'radio_dcdc',
                label: 'DCDC enabled',
                type: 'checkbox',
                icon: null,
                desc: 'Use the SX1280 DC-DC converter rather than LDO voltage regulator (15uH inductor must be present)'
            },
            /* /FEATURE: NOT IS_SX127X */
            /* FEATURE: IS_SX127X */
            {
                id: 'radio_rfo_hf',
                label: 'RFO_HF enabled',
                type: 'checkbox',
                icon: null,
                desc: 'SX127x PA to use, either the RFO_HF or PA_BOOST (depends on circuit design)'
            },
            /* /FEATURE: IS_SX127X */
            /* FEATURE: IS_LR1121 */
            {
                id: 'radio_rfsw_ctrl',
                label: 'LR1121 RF Switch Controls',
                type: 'text',
                size: 40,
                className: 'array',
                icon: null,
                desc: 'Comma-separated list of 8 values used for setting the LR1121 RF switch controls'
            },
            /* /FEATURE: IS_LR1121 */
        ]
    },

    {
        title: 'Radio Antenna', rows: [
            {
                id: 'ant_ctrl',
                label: 'CTRL pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Pin connected to Antenna select pin on power amplifier'
            },
            {
                id: 'ant_ctrl_compl',
                label: 'CTRL_COMPL pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Inverted CTRL for devices using antenna selectors that need separate pins for A/B selection'
            },
        ]
    },

    {
        title: 'Radio Power', rows: [
            {
                id: 'power_enable',
                label: 'PA enable pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Enable the power amplifier (active high)'
            },
            {
                id: 'power_apc2',
                label: 'APC2 pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Power amplifier control voltage'
            },
            {
                id: 'power_rxen',
                label: 'RXEN pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Enable RX mode LNA (active high)'
            },
            {
                id: 'power_txen',
                label: 'TXEN pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Enable TX mode PA (active high)'
            },
            {
                id: 'power_rxen_2',
                label: 'RXEN_2 pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Enable RX mode LNA on second SX1280 (active high)'
            },
            {
                id: 'power_txen_2',
                label: 'TXEN_2 pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Enable TX mode PA on second SX1280 (active high)'
            },
            {
                id: 'power_min', label: 'Min Power', type: 'select', options: [
                    {value: 0, label: '10mW'}, {value: 1, label: '25mW'}, {value: 2, label: '50mW'}, {
                        value: 3,
                        label: '100mW'
                    }, {value: 4, label: '250mW'}, {value: 5, label: '500mW'}, {value: 6, label: '1000mW'}, {
                        value: 7,
                        label: '2000mW'
                    }
                ], desc: 'Minimum selectable power output'
            },
            {
                id: 'power_high', label: 'High Power', type: 'select', options: [
                    {value: 0, label: '10mW'}, {value: 1, label: '25mW'}, {value: 2, label: '50mW'}, {
                        value: 3,
                        label: '100mW'
                    }, {value: 4, label: '250mW'}, {value: 5, label: '500mW'}, {value: 6, label: '1000mW'}, {
                        value: 7,
                        label: '2000mW'
                    }
                ], desc: 'Highest selectable power output (if option for higher power is NOT enabled)'
            },
            {
                id: 'power_max', label: 'Max Power', type: 'select', options: [
                    {value: 0, label: '10mW'}, {value: 1, label: '25mW'}, {value: 2, label: '50mW'}, {
                        value: 3,
                        label: '100mW'
                    }, {value: 4, label: '250mW'}, {value: 5, label: '500mW'}, {value: 6, label: '1000mW'}, {
                        value: 7,
                        label: '2000mW'
                    }
                ], desc: "Absolute maximum selectable power output (only available if 'higher power' option is enabled)"
            },
            {
                id: 'power_default', label: 'Default Power', type: 'select', options: [
                    {value: 0, label: '10mW'}, {value: 1, label: '25mW'}, {value: 2, label: '50mW'}, {
                        value: 3,
                        label: '100mW'
                    }, {value: 4, label: '250mW'}, {value: 5, label: '500mW'}, {value: 6, label: '1000mW'}, {
                        value: 7,
                        label: '2000mW'
                    }
                ], desc: 'Default power output when resetting or first flashing a module'
            },
            {
                id: 'power_control', label: 'Power Level control', type: 'select', options: [
                    {value: 0, label: 'via SEMTECH'}, {value: 3, label: 'via ESP DACWRITE'}
                ], desc: 'How the power level is set'
            },
            {
                id: 'power_values',
                label: 'Power Value(s)',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'Comma-separated list of values that set the power output (if using a DAC these are the DAC values)'
            },
            {
                id: 'power_values2',
                label: 'Secondary Power Value(s)',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'Comma-separated list of values that set the power output (if using a DAC then these set the Semtech power output)'
            },
            {
                id: 'power_values_dual',
                label: 'Dual Power Value(s)',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'Comma-separated list of values that set the higher frequency power output of a dual band Tx/Rx'
            },
            {
                id: 'power_lna_gain',
                label: 'PA LNA Gain',
                type: 'text',
                size: 20,
                desc: 'The amount of dB gain provided by the LNA'
            },
        ]
    },

    {
        title: 'Radio Power Detection', rows: [
            {
                id: 'power_pdet',
                label: 'PDET pin',
                type: 'text',
                size: 3,
                icon: 'analog',
                desc: "Analog input (up to 1.1V) connected to 'power detect' pin on PA for adjustment of the power output"
            },
            {
                id: 'power_pdet_intercept',
                label: 'Intercept',
                type: 'text',
                size: 20,
                desc: 'Intercept and Slope are used together to calculate the dBm from the measured mV on the PDET pin'
            },
            {
                id: 'power_pdet_slope',
                label: 'Slope',
                type: 'text',
                size: 20,
                desc: 'dBm = mV * slope + intercept, this is then used to adjust the actual output power accordingly'
            },
        ]
    },

    /* FEATURE: IS_TX */
    {
        title: 'Analog Joystick', rows: [
            {
                id: 'joystick',
                label: 'ADC pin',
                type: 'text',
                size: 3,
                icon: 'analog',
                desc: 'Analog Input (3.3V max) use to read joystick direction using a resistor network'
            },
            {
                id: 'joystick_values',
                label: 'Values',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'Comma-separated list of ADC values (12-bit) for UP, DOWN, LEFT, RIGHT, ENTER, IDLE'
            },
        ]
    },

    {
        title: 'Digital Joystick', rows: [
            {
                id: 'five_way1',
                label: 'Pin 1',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'These 3 pins create a binary value for the joystick direction'
            },
            {id: 'five_way2', label: 'Pin 2', type: 'text', size: 3, icon: 'input', desc: '7 = IDLE, 6 = OK, 5 = DOWN'},
            {
                id: 'five_way3',
                label: 'Pin 3',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: '4 = RIGHT, 3 = UP, 2 = LEFT'
            },
        ]
    },
    /* /FEATURE: IS_TX */

    {
        title: 'Mood Lighting', rows: [
            {
                id: 'led_rgb',
                label: 'RGB LED pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Signal pin for WS2812 RGB LED or LED strip'
            },
            {
                id: 'led_rgb_isgrb',
                label: 'RGB LED is GRB',
                type: 'checkbox',
                desc: 'Most WS2812 RGB LEDs are actually GRB'
            },
            {
                id: 'ledidx_rgb_status',
                label: 'RGB indexes for Status',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'Indexes into the "string" of RGB LEDs (if empty then only LED at 0 is used)'
            },
            /* FEATURE: NOT IS_TX */
            {
                id: 'ledidx_rgb_vtx',
                label: 'RGB indexes for VTX Status',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'Indexes into the "string" of RGB LEDs (if empty then no VTX status)'
            },
            /* /FEATURE: NOT IS_TX */
            {
                id: 'ledidx_rgb_boot',
                label: 'RGB indexes for Boot animation',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'Indexes into the "string" of RGB LEDs (if empty status indexes are used)'
            },
            {
                id: 'led',
                label: 'LED pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Only use when only a single LED is used'
            },
            {
                id: 'led_red',
                label: 'Red LED pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'If there are multiple LEDs, then this is the pin for the RED LED'
            },
            {
                id: 'led_red_invert',
                label: 'Red LED inverted',
                type: 'checkbox',
                desc: 'LEDs are active LOW unless this is checked'
            },
            {
                id: 'led_green',
                label: 'Green LED pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'If there is a GREEN LED as well as RED above'
            },
            {
                id: 'led_green_invert',
                label: 'Green LED inverted',
                type: 'checkbox',
                desc: 'Check if the LED is active HIGH'
            },
            {
                id: 'led_blue',
                label: 'Blue LED pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Pin for a 3rd, BLUE, LED!'
            },
            {
                id: 'led_blue_invert',
                label: 'Blue LED inverted',
                type: 'checkbox',
                desc: 'Check if the LED is active HIGH'
            },
        ]
    },

    {
        title: 'Button(s)', rows: [
            {
                id: 'button',
                label: 'Button 1 pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Single/first (active low) button'
            },
            {
                id: 'button_led_index',
                label: 'Button 1 RGB Index',
                type: 'text',
                size: 3,
                desc: 'Index of button LED in RGB string, leave empty for no RGB LED'
            },
            {
                id: 'button2',
                label: 'Button 2 pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Second (active low) button'
            },
            {
                id: 'button2_led_index',
                label: 'Button 2 RGB Index',
                type: 'text',
                size: 3,
                desc: 'Index of button LED in RGB string, leave empty for no RGB LED'
            },
        ]
    },

    /* FEATURE: IS_TX */
    {
        title: 'OLED/TFT (Crotch TV)', rows: [
            {
                id: 'screen_type', label: 'Screen type', type: 'select', options: [
                    {value: 0, label: 'None'}, {value: 1, label: 'I2C OLED (SSD1306 128x64)'}, {
                        value: 2,
                        label: 'SPI OLED (SSD1306 128x64)'
                    }, {value: 3, label: 'SPI OLED (small SSD1306 128x32)'}, {
                        value: 4,
                        label: 'SPI TFT (ST7735 160x80)'
                    }
                ], desc: 'Type of OLED connected'
            },
            {
                id: 'screen_reversed',
                label: '180 rotation',
                type: 'checkbox',
                desc: 'Select to rotate the display 180 degrees'
            },
            {
                id: 'screen_cs',
                label: 'CS pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Chip Select (if using SPI)'
            },
            {
                id: 'screen_dc',
                label: 'DC pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Data/Command Select (if using SPI)'
            },
            {id: 'screen_mosi', label: 'MOSI pin', type: 'text', size: 3, icon: 'output', desc: 'Data (if using SPI)'},
            {id: 'screen_rst', label: 'RST pin', type: 'text', size: 3, icon: 'output', desc: 'Reset'},
            {
                id: 'screen_sck',
                label: 'SCK pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Clock (either SPI or I2C)'
            },
            {id: 'screen_sda', label: 'SDA pin', type: 'text', size: 3, icon: 'input-output', desc: 'Data (I2C)'},
            {id: 'screen_bl', label: 'BL pin', type: 'text', size: 3, icon: 'output', desc: 'Backlight'},
        ]
    },

    {
        title: 'Backpack / Logging', rows: [
            {id: 'use_backpack', label: 'Enable Backpack', type: 'checkbox', desc: 'If a TX backpack is connected'},
            {
                id: 'debug_backpack_baud',
                label: 'Baud Rate',
                type: 'text',
                size: 10,
                desc: 'Baud rate used to communicate to the backpack (normally 460800)'
            },
            {
                id: 'debug_backpack_rx',
                label: 'RX pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Connected to TX pin on backpack'
            },
            {
                id: 'debug_backpack_tx',
                label: 'TX pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Connected to RX pin on backpack'
            },
            {
                id: 'backpack_boot',
                label: 'BOOT pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Pin connected to GPIO0 pin on backpack ESP8285, allows passthrough flashing'
            },
            {
                id: 'backpack_en',
                label: 'EN pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Pin connected to EN pin on backpack ESP8285, allows passthrough flashing'
            },
            {
                id: 'passthrough_baud',
                label: 'Passthrough baud',
                type: 'text',
                size: 7,
                desc: 'Baud rate to flash the backpack ESP8285 (default is to use the baud rate above)'
            },
        ]
    },

    {
        title: 'I2C & Misc Devices', rows: [
            {
                id: 'i2c_scl',
                label: 'SCL pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'I2C clock pin used to communicate with I2C devices (may be the same as OLED I2C)'
            },
            {
                id: 'i2c_sda',
                label: 'SDA pin',
                type: 'text',
                size: 3,
                icon: 'input-output',
                desc: 'I2C data pin used to communicate with I2C devices (may be the same as OLED I2C)'
            },
            {
                id: 'misc_fan_en',
                label: 'Fan enable pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Pin used to enable a cooling FAN (active HIGH)'
            },
            {
                id: 'misc_fan_pwm',
                label: 'Fan PWM pin',
                type: 'text',
                size: 3,
                icon: 'pwm',
                desc: 'If the fan is controlled by PWM'
            },
            {
                id: 'misc_fan_speeds',
                label: 'Fan PWM output values',
                type: 'text',
                size: 40,
                className: 'array',
                desc: 'If the fan is PWM controlled, then this is the list of values for the PWM output for the matching power output levels'
            },
            {
                id: 'misc_fan_tacho',
                label: 'Fan TACHO pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'If the fan has a "tachometer" interrupt pin'
            },
            {
                id: 'gsensor_stk8xxx',
                label: 'Has STK8xxx G-sensor',
                type: 'checkbox',
                desc: 'Checked if there is a STK8xxx g-sensor on the I2C bus'
            },
            {
                id: 'misc_gsensor_int',
                label: 'G-sensor interrupt pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'Pin connected the STK8xxx g-sensor for interrupts'
            },
            {
                id: 'thermal_lm75a',
                label: 'Has LM75A Thermal sensor',
                type: 'checkbox',
                desc: 'Checked if there is a LM75A thermal sensor on the I2C bus'
            },
        ]
    },
    /* /FEATURE: IS_TX */

    /* FEATURE: NOT IS_TX */
    {
        title: 'PWM', rows: [
            {
                id: 'pwm_outputs',
                label: 'PWM output pins',
                type: 'text',
                size: 40,
                className: 'array',
                icon: 'pwm',
                desc: 'Comma-separated list of pins used for PWM output'
            },
        ]
    },

    {
        title: 'VBat', rows: [
            {
                id: 'vbat',
                label: 'VBat pin',
                type: 'text',
                size: 3,
                icon: 'analog',
                desc: 'Analog input pin for reading VBAT voltage (1V max on 8285, 3.3V max on ESP32)'
            },
            {
                id: 'vbat_offset',
                label: 'VBat offset',
                type: 'text',
                size: 7,
                desc: 'Offset and scale are used together with the analog pin to calculate the voltage'
            },
            {id: 'vbat_scale', label: 'VBat scale', type: 'text', size: 7, desc: 'voltage = (analog - offset) / scale'},
            {
                id: 'vbat_atten', label: 'VBat attenuation', type: 'select', options: [
                    {value: -1, label: 'Default'}, {value: 0, label: '0 dB'}, {value: 1, label: '2.5 dB'}, {
                        value: 2,
                        label: '6 dB'
                    }, {value: 3, label: '11 dB'}, {value: 4, label: '0 dB + calibration'}, {
                        value: 5,
                        label: '2.5 dB + calibration'
                    }, {value: 6, label: '6 dB + calibration'}, {value: 7, label: '11 dB + calibration'}
                ], desc: 'ADC pin attenuation (ESP32) and optional efuse-based calibration adjustment'
            },
        ]
    },

    {
        title: 'SPI VTX', rows: [
            {
                id: 'vtx_amp_pwm',
                label: 'RF amp PWM pin',
                type: 'text',
                size: 3,
                icon: 'pwm',
                desc: 'Set the power output level of the VTX PA (value is calculated based on power and frequency using VPD interpolation values)'
            },
            {
                id: 'vtx_amp_vpd',
                label: 'RF amp VPD pin',
                type: 'text',
                size: 3,
                icon: 'analog',
                desc: 'Analog input for VPD (power detect) from VTX PA'
            },
            {
                id: 'vtx_amp_vref',
                label: 'RF amp VREF pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Active high enable pin the the VTX PA VREF (voltage reference)'
            },
            {
                id: 'vtx_nss',
                label: 'SPI NSS pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Chip select for RTC6705 VTx (leave undefined if sharing Radio SPI bus)'
            },
            {
                id: 'vtx_sck',
                label: 'SPI SCK pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'Clock pin on RTC6705 VTx (leave undefined if sharing Radio SPI bus)'
            },
            {
                id: 'vtx_miso',
                label: 'SPI MISO pin',
                type: 'text',
                size: 3,
                icon: 'input',
                desc: 'MISO pin on RTC6705 VTx (leave undefined if sharing Radio SPI bus)'
            },
            {
                id: 'vtx_mosi',
                label: 'SPI MOSI pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'MOSI pin on RTC6705 VTx (leave undefined if sharing Radio SPI bus)'
            },
            {
                id: 'vtx_amp_vpd_25mW',
                label: '25mW VPD interpolation values',
                type: 'text',
                size: 40,
                className: 'array',
                desc: '4 values for 5650, 5750, 5850, 5950 frequencies at 25mW'
            },
            {
                id: 'vtx_amp_vpd_100mW',
                label: '100mW VPD interpolation values',
                type: 'text',
                size: 40,
                className: 'array',
                desc: '4 values for 5650, 5750, 5850, 5950 frequencies at 100mW'
            },
            {
                id: 'vtx_amp_pwm_25mW',
                label: '25mW PWM interpolation values',
                type: 'text',
                size: 40,
                className: 'array',
                desc: '4 values for 5650, 5750, 5850, 5950 frequencies at 25mW'
            },
            {
                id: 'vtx_amp_pwm_100mW',
                label: '100mW PWM interpolation values',
                type: 'text',
                size: 40,
                className: 'array',
                desc: '4 values for 5650, 5750, 5850, 5950 frequencies at 100mW'
            },
        ]
    },

    {
        title: 'I2C', rows: [
            {
                id: 'i2c_scl',
                label: 'SCL pin',
                type: 'text',
                size: 3,
                icon: 'output',
                desc: 'I2C clock pin used to communicate with I2C devices'
            },
            {
                id: 'i2c_sda',
                label: 'SDA pin',
                type: 'text',
                size: 3,
                icon: 'input-output',
                desc: 'I2C data pin used to communicate with I2C devices'
            },
        ]
    },
    /* /FEATURE: NOT IS_TX */
];

export default HARDWARE_SCHEMA;
