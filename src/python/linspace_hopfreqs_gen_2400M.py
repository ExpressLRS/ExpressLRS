import numpy as np

start = 2400000000
stop = 2450000000
bandwidth = 800000
numfreqs = 50
stopband = bandwidth*0.25

output = np.linspace(start, stop, numfreqs)


prev = start + bandwidth/2 +0
startf = prev

for x in range(0,numfreqs):
    print(prev)
    prev = prev + bandwidth + stopband


print(startf-(start+bandwidth/2))
print(stop-(prev+bandwidth/2))
