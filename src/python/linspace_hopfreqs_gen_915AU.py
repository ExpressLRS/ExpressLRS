import numpy as np

start = 915000000
stop = 928000000
bandwidth = 500000
numfreqs = 20
stopband = 100000

output = np.linspace(start, stop, numfreqs)


prev = start + bandwidth/2 +250000
startf = prev

for x in range(0,numfreqs):
    print(prev)
    prev = prev + bandwidth + stopband


print(startf-(start+bandwidth/2))
print(stop-(prev+bandwidth/2))
