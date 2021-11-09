return {
    {name='Packet Rate', id=0, type=9, values={'250(-108dBm)','500(-105dBm)'}, value=1, unit='Hz'},
    {name='Telem Ratio', id=1, type=9, values={'Off','1:128'}, value=1, unit=''},
    {name='Switch Mode', id=2, type=9, values={'Hybrid','Wide'}, value=1, unit=''},
    {name='Model Match', id=3, type=9, values={'Off','On'}, value=0, unit=''},
    {name='TX Power', id=4, type=11},
      {name='Max Power', id=5, type=9, parent=4, values={'10','25','50'}, value=2, unit='mW'},
      {name='Dynamic', id=6, type=9, parent=4, values={'Off','On','AUX9'}, value=1, unit=''},
      {name='Fan Thresh', id=6, type=9, parent=4, values={'10mW','25mW','50mW', '250mW'}, value=3, unit=''},
    {name='VTX Administrator', id=7, type=11},
      {name='Band', id=8, type=9, parent=7, values={'Off', 'A', 'B', 'F', 'R', 'L'}, value=0, unit=''},
      {name='Channel', id=9, type=9, parent=7, values={'1', '2', '3', '4' }, value=0, unit=''},
      {name='Pwr Lvl', id=10, type=9, parent=7, values={'-', '1', '2', '3' }, value=0, unit=''},
      {name='Pitmode', id=11, type=9, parent=7, values={'Off', 'On'}, value=0, unit=''},
      {name='Send VTx', id=12, type=13, parent=7},
    {name='Bind', id=8, type=13},
    {name='Wifi Update', id=9, type=13},
    {name='BLE Joystick', id=10, type=13},
    {name='master', id=11, type=12, value='f00fcb'},

    {name="----BACK----", id=12, type=14, parent=255}
  }