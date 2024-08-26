return {
  {name='Packet Rate', id=0, type=9, values={'250(-108dBm)','500(-105dBm)'}, value=1, unit='Hz'},
  {name='Telem Ratio', id=1, type=9, values={'Off','1:128'}, value=1, unit=''},
  {name='Switch Mode', id=2, type=9, values={'Hybrid','Wide'}, value=1, unit=''},
  {name='Model Match', id=3, type=9, values={'Off',''}, grey=true, value=0, unit='(ID:1)'},
  {name='TX Power', id=4, type=11},
    {name='Max Power', id=5, type=9, parent=4, values={'10','25','50'}, value=2, unit='mW'},
    {name='Dynamic', id=6, type=9, parent=4, values={'Off','On','AUX9'}, value=1, unit=''},
    {name='Fan Thresh', id=7, type=9, parent=4, values={'10mW','25mW','50mW', '250mW'}, value=3, unit=''},
  {name='VTX Administrator', id=8, type=11},
    {name='Band', id=9, type=9, parent=8, values={'Off', 'A', 'B', 'F', 'R', 'L'}, value=0, unit=''},
    {name='Channel', id=10, type=0, parent=8, value=1, step=1, min=1, max=8, unit=''},
    {name='Pwr Lvl', id=11, type=9, parent=8, values={'-', '1', '2', '3' }, value=0, unit=''},
    {name='Pitmode', id=12, type=9, parent=8, values={'Off', 'On'}, value=0, unit=''},
    {name='Send VTx', id=13, type=13, parent=8},
  {name='Bind', id=14, type=13},
  {name='Wifi Update', id=15, type=13},
  {name='BLE Joystick', id=16, type=13},
  {name='master', id=17, type=16, value='f00fcb'},
  {name='Float Tst', id=18, type=8, value=-15, step=5, prec=1000, min=-50, max=50, unit='flt', fmt='%.3fflt'},

  {name="----BACK----", type=14, parent=255},
  {name="----EXIT----", type=14, exit = true}
}, "0/500   C", "ExpressLRS TX"