f = open("trial2.txt", "r")
lastroute=0
lastrssi=0
for x in f:
  #print(x)
  s=x.split("&")
  time=s[0]
  q=s[1].split(":")
  #print(q)
  if(len(q)>1):
      #print(q)
      if(q[0]=='routing'):
          a=s[1]
          #print('ROUTE: ',a)
          aq=a.split('[',)[1].split(',')
          lastroute=aq[4].split(':')[1]
          #print('n3:',aq[4].split(':')[1])
          #print('r3:',aq[5].split(':')[1].strip('}]\r\n'))
          lastrssi=aq[5].split(':')[1].strip('}]\r\n')
      else:
          a=s[1]
          #print('GPS: ', a)
          #print(len(a.split(',')))
          if(len(a.split(','))==2):
          #if(a.split(':')[0]=='node'):
              coord=a.split('[')[1].split(']')[0].split(',')
              #print(coord)
              lat=coord[0]
              #print(lat)
              lon=coord[1]
              #print("GPS -- lat,lon:",lat,lon)
              print({'time':time,'lat':lat,'lon':lon,'lastroute':lastroute,'lastrssi':lastrssi})
          #lat=float(coord[0])
          #lon=float(coord[1])
