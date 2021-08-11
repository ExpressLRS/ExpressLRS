# importing only those functions
# which are needed
from tkinter import * 
from tkinter.ttk import *
from tkinter import font as tkFont  # for convenience
import sys

def passthrough():
    root.destroy()


rel_path = str(sys.argv[0].split("flash_method_gui.py")[0])
print(rel_path)
  
# creating tkinter window
root = Tk()

  
# Adding widgets to the root window
Label(root, text = 'Select Upload Method:', font =(
  'Verdana', 15)).pack(side = TOP, pady = 10)

icon_subsample_x = 1
icon_subsample_y = 1

path_otx = rel_path + "icons\img_otx.png"
path_stlink = rel_path + "icons\img_stlink.png"
path_ftdi = rel_path + "icons\img_ftdi.png"
path_fc = rel_path + "icons\img_fc.png"
path_wifi = rel_path + "icons\img_wifi.png"

print(path_ftdi)
  
# Creating a photoimage object to use image
photo_otx    = PhotoImage(file = path_otx).subsample(icon_subsample_x, icon_subsample_y)
photo_stlink = PhotoImage(file = path_stlink).subsample(icon_subsample_x, icon_subsample_y)
photo_ftdi   = PhotoImage(file = path_ftdi).subsample(icon_subsample_x, icon_subsample_y)
photo_fc     = PhotoImage(file = path_fc).subsample(icon_subsample_x, icon_subsample_y)
photo_wifi   = PhotoImage(file = path_wifi).subsample(icon_subsample_x, icon_subsample_y)

helv36 = tkFont.Font(family='Helvetica', size=36, weight='bold')
 
# make the buttons 
button_otx = Button(root, text = 'Copy file to OpenTX', image = photo_otx, compound = TOP).pack(side = LEFT)
button_stlink = Button(root, text = 'St-Link V2 Programmer', image = photo_stlink, compound = TOP).pack(side = LEFT)
button_ftdi = Button(root, text = 'USB-Serial Adapter', image = photo_ftdi, compound = TOP).pack(side = LEFT)
button_pass = Button(root, text = 'Flight Controller Passthrough', image = photo_fc, compound = TOP, command=passthrough).pack(side = LEFT)
button_wifi = Button(root, text = 'Wireless', image = photo_wifi, compound = TOP).pack(side = LEFT)

# Tkinter way to find the screen resolution
screen_width = root.winfo_screenwidth()
screen_height = root.winfo_screenheight()

size = tuple(int(_) for _ in root.geometry().split('+')[0].split('x'))
x = screen_width/2 - size[0]/2
y = screen_height/2 - size[1]/2

root.geometry("+%d+%d" % (x-500, y-250))
root.title("Select Upload Method!")
root.attributes('-topmost', True)
root.update()
  
mainloop()
