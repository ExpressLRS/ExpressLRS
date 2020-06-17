import random
import matplotlib.pyplot as plt
import math

X = []
length = 256

n = 20
limit = math.floor(length/n)
print(limit)

prev_val = 0
rand = 0

last_zero = 0
last_zero_interval = n

tracker = [0 for x in range(n+1)]

for i in range(0,length):

    if(i >= last_zero + last_zero_interval):
        rand = 0
        last_zero = i
        
    else:
            
        while(prev_val == rand or tracker[rand] >= limit or rand > n):
            rand = random.randint(1,n)

    
    X.append(rand)
    print(0)
    tracker[rand] = tracker[rand] + 1
    prev_val = rand

print(X)

plt.hist(X, bins=n+1)  # arguments are passed to np.histogram
plt.show()




    
