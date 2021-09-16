return {
    {name='Packet Rate', id=0, type=9, parent=0, values={'250(-108dBm)','500(-105dBm)'}, value=1, min=0, max=1, default=0, unit='Hz'},
    {name='Telem Ratio', id=1, type=9, parent=0, values={'Off','1:128'}, value=1, min=0, max=1, default=0, unit=''},
    {name='Switch Mode', id=2, type=9, parent=0, values={'Hybrid','Wide'}, value=1, min=0, max=1, default=0, unit=''},
    {name='Model Match', id=3, type=9, parent=0, values={'Off','On'}, value=0, min=0, max=1, default=0, unit=''},
    {name='TX Power', id=4, type=11, parent=0},
    {name='VTX Administrator', id=5, type=11, parent=0},
    {name='Bind', id=6, type=13, parent=0},
    {name='Wifi Update', id=7, type=13, parent=0},
    {name='BLE Joystick', id=7, type=13, parent=0},
    {name='master', id=7, type=12, parent=0, value='f00fcb'},

    {name="----BACK----", id=2, type=14, parent=255}
  }