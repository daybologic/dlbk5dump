#!/usr/bin/env python3
"""Count BIRT/BAPM/DEAT/BURI events with a PLAC, and those with no DATE."""

import re,sys
L=open(sys.argv[1],encoding='utf-8').read().split('\n')
po=tot=0
for i,l in enumerate(L):
  if re.match(r'^1 (BIRT|BAPM|DEAT|BURI)$',l):
    sub=[];j=i+1
    while j<len(L) and L[j].startswith('2 '): sub.append(L[j][2:6]);j+=1
    if 'PLAC' in sub: tot+=1
    if 'PLAC' in sub and 'DATE' not in sub: po+=1
print('events with PLAC',tot,'place-only',po)
