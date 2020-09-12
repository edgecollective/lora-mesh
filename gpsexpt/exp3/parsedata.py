f = open("mesh.txt", "r")
lastroute=0
lastrssi=0
linecount=0
lat='0'
lon='0'
print('[')
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
    print('{\"time\":\"'+time+'\",\"node\":\"'+node+'\",\"lastroute\":\"'+lastroute+'\",\"lastrssi\":\"'+lastrssi+'\",\"lon\":\"'+lon+'\",\"lat\":\"'+lat+'\"},')
print(']')
