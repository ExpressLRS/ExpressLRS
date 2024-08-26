#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
A simple http server for testing/debugging the web-UI

open http://localhost:8080/
add the following query params for TX and/or 900Mhz testing
    isTX
    hasSubGHz
"""

from external.bottle import route, run, response, request
from external.wheezy.template.engine import Engine
from external.wheezy.template.ext.core import CoreExtension
from external.wheezy.template.loader import FileLoader

net_counter = 0
isTX = False
hasSubGHz = False
is8285 = True
chip = 'LR1121'

config = {
        "options": {
            "uid": [1,2,3,4,5,6],   # this is the 'flashed' UID and may be empty if using traditional binding on an RX.
            "tlm-interval": 240,
            "fan-runtime": 30,
            "no-sync-on-arm": False,
            "uart-inverted": True,
            "unlock-higher-power": False,
            "is-airport": True,
            "rcvr-uart-baud": 400000,
            "rcvr-invert-tx": False,
            "lock-on-first-connection": True,
            "domain": 1,
            # "wifi-on-interval": 60,
            "wifi-password": "w1f1-pAssw0rd",
            "wifi-ssid": "network-ssid"
        },
        "config": {
            "uid": [1,2,3,4,5,6],   # this is the 'running' UID
            "uidtype": "On loan",
            "ssid":"network-ssid",
            "mode":"STA",
            "modelid":255,
            "pwm":[
                {
                    # 10fs 4ch 1inv 4mode 1narrow
                    "config": 0 + 0<<10 + 0<14 + 0<<15 + 0<<19,
                    "pin": 0,
                    "features": 12
                },
                {
                    "config": 1536,
                    "pin": 4,
                    "features": 12 + 16
                },
                {
                    "config": 2048,
                    "pin": 5,
                    "features": 12 + 16
                },
                {
                    "config": 3584,
                    "pin": 1,
                    "features": 1 + 16
                },
                {
                    "config": 4608,
                    "pin": 3,
                    "features": 2 + 16
                }
            ],
            "serial-protocol": 3,
            "sbus-failsafe": 0,
            "product_name": "Generic ESP8285 + 5xPWM 2.4Ghz RX",
            "lua_name": "ELRS+PWM 2400RX",
            "reg_domain": "ISM2G4",
            "button-actions": [
                {
                    "color" : 255,
                    "action": [
                        {
                            "is-long-press": False,
                            "count": 3,
                            "action": 6
                        },
                        {
                            "is-long-press": True,
                            "count": 5,
                            "action": 1
                        }
                    ]
                },
                {
                    #"color" : 224, # No color for you button 2
                    "action": [
                        {
                            "is-long-press": False,
                            "count": 2,
                            "action": 3
                        },
                        {
                            "is-long-press": True,
                            "count": 0,
                            "action": 4
                        }
                    ]
                }
            ]
        }
    }

def apply_template(mainfile):
    global isTX, hasSubGHz, chip, is8285
    if(isTX):
        platform = 'Unified_ESP32_2400_TX'
        is8285 = False
    else:
        platform = 'Unified_ESP8285_2400_RX'
        is8285 = True
    engine = Engine(
        loader=FileLoader(["html"]),
        extensions=[CoreExtension("@@")]
    )
    template = engine.get_template(mainfile)
    data = template.render({
            'VERSION': 'testing (xxxxxx)',
            'PLATFORM': platform,
            'isTX': isTX,
            'hasSubGHz': hasSubGHz,
            'chip': chip,
            'is8285': is8285
        })
    return data

@route('/')
def index():
    global net_counter, isTX, hasSubGHz, chip, is8285
    net_counter = 0
    isTX = 'isTX' in request.query
    hasSubGHz = 'hasSubGHz' in request.query
    if 'chip' in request.query:
        chip = request.query['chip']
    response.content_type = 'text/html; charset=latin9'
    return apply_template('index.html')

@route('/elrs.css')
def elrs():
    response.content_type = 'text/css; charset=latin9'
    return apply_template('elrs.css')

@route('/scan.js')
def scan():
    response.content_type = 'text/javascript; charset=latin9'
    return apply_template('scan.js')

@route('/mui.js')
def mui():
    response.content_type = 'text/javascript; charset=latin9'
    return apply_template('mui.js')

@route('/hardware.html')
def hardware_html():
    response.content_type = 'text/html; charset=latin9'
    return apply_template('hardware.html')

@route('/hardware.js')
def hardware_js():
    response.content_type = 'text/javascript; charset=latin9'
    return apply_template('hardware.js')

@route('/cw.html')
def cw_html():
    global chip
    if 'chip' in request.query:
        chip = request.query['chip']
    response.content_type = 'text/html; charset=latin9'
    return apply_template('cw.html')

@route('/cw.js')
def cw_js():
    response.content_type = 'text/javascript; charset=latin9'
    return apply_template('cw.js')

@route('/cw')
def cw():
    response.content_type = 'application/json; charset=latin9'
    return '{"radios": 2, "center": 915000000, "center2": 2440000000}'

@route('/lr1121.html')
def lr1121_html():
    response.content_type = 'text/html; charset=latin9'
    return apply_template('lr1121.html')

@route('/lr1121.js')
def lr1121_js():
    response.content_type = 'text/javascript; charset=latin9'
    return apply_template('lr1121.js')

@route('/lr1121.json')
def lr1121_json():
    return {
        "radio1": {
            "hardware": 34,
            "type": 3,
            "firmware": 259
        },
        "radio2": {
            "hardware": 34,
            "type": 3,
            "firmware": 257
        }
    }

@route('/lr1121', method="POST")
def lr1121_upload():
    return '{ "status": "ok", "msg": "All good!" }'

@route('/config')
def options():
    response.content_type = 'application/json; charset=latin9'
    return config

@route('/config', method='POST')
def update_config():
    if 'button-actions' in request.json:
        config['config']['button-actions'] = request.json['button-actions']
    if 'pwm' in request.json:
        i=0
        for x in request.json['pwm']:
            print(x)
            config['config']['pwm'][i]['config'] = x
            i = i + 1
    if 'protocol' in request.json:
        config['config']['serial-protocol'] = request.json['protocol']
    if 'modelid' in request.json:
        config['config']['modelid'] = request.json['modelid']
    if 'forcetlm' in request.json:
        config['config']['force-tlm'] = request.json['forcetlm']
    return "Config Updated"

@route('/options.json', method='POST')
def update_options():
    config['options'] = request.json
    return "Options Updated"

@route('/import', method='POST')
def import_config():
    json = request.json
    print(json)
    return "Config Updated"

@route('/sethome', method='POST')
def options():
    response.content_type = 'application/json; charset=latin9'
    return "Connecting to network '" + request.forms.get('network') + "', connect to http://elrs_tx.local from a browser on that network"

@route('/networks.json')
def mode():
    global net_counter
    net_counter = net_counter + 1
    if (net_counter > 3):
        return '["Test Network 1", "Test Network 2", "Test Network 3", "Test Network 4", "Test Network 5"]'
    response.status = 204
    return '[]'

if __name__ == '__main__':
    run(host='localhost', port=8080)
