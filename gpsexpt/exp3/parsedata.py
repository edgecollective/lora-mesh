def file_len(fname):
    with open(fname) as f:
        for i, l in enumerate(f):
            pass
    return i + 1

filename = "mesh.txt"

numlines=file_len(filename)

f = open("mesh.txt", "r")
lastroute=0
lastrssi=0
linecount=0
lat='0'
lon='0'

linecount=0

print('[',end='')
for x in f:
  #print(x)
  s=x.split("&")
  time=s[0]
  q=s[1].split(";")
  #print(len(q))
  if len(q)==3:
    node=q[0].strip('[').strip(']').split(':')[1]
    gps=q[1].strip('[').strip(']').split(',')
    lat=gps[0]
    lon=gps[1]
    route=q[2].strip('[').strip(']').strip('\r').strip('\n').strip()
    #print(node)
    #print(lat,lon)
    node1stats=route.split('}')[0].strip('{').split(',')
    lastroute=node1stats[0].split(':')[1]
    lastrssi=node1stats[1].split(':')[1]
    #print(node1stats)
    #print(relaynode,node1rssi)
    if ((float(lat)!=0.0) and (float(lon)!=0.0)):
        if(linecount!=0):
            print(',',end='')
        print('{\"time\":\"'+time+'\",\"node\":\"'+node+'\",\"lastroute\":\"'+lastroute+'\",\"lastrssi\":\"'+lastrssi+'\",\"lon\":\"'+lon+'\",\"lat\":\"'+lat+'\"}',end='')
        linecount=linecount+1
        #if (linecount<numlines):
        #print(',',end='')
        #print(linecount)
#linecount=linecount+1
print(']')
