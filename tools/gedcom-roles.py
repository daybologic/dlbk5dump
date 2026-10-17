#!/usr/bin/env python3
"""Flag children born long before their family's marriage, or close in age
to a spouse in that family (likely spouses demoted to children)."""

import re,sys
recs={};cur=None
for l in open(sys.argv[1],encoding='utf-8').read().split('\n'):
    m=re.match(r'^0 (@[^@]+@) (\w+)',l)
    if m: cur=m[1];recs[cur]=[];continue
    if l.startswith('0 '):cur=None;continue
    if cur:recs[cur].append(l)
def yr(r,ev):
    for i,l in enumerate(r):
        if l=='1 '+ev:
            for x in r[i+1:]:
                if not x.startswith('2 '):break
                m=re.match(r'2 DATE .*?(\d{4})',x)
                if m:return int(m[1])
def name(p):return next((l[7:] for l in recs[p] if l.startswith('1 NAME')),'?')
def by(p):return yr(recs[p],'BIRT') or yr(recs[p],'BAPM')
onesp=0
for f,r in recs.items():
    if not f.startswith('@F'):continue
    sp=[l.split()[2] for l in r if l.startswith(('1 HUSB','1 WIFE'))]
    if len(sp)==1: onesp+=1
    m=yr(r,'MARR')
    for c in [l.split()[2] for l in r if l.startswith('1 CHIL')]:
        b=by(c);sps=[by(s) for s in sp];flag=[]
        if m and b and b<m-15: flag.append(f'born {b}, {m-b}y before marriage {m}')
        if b and any(s and abs(s-b)<12 for s in sps): flag.append(f'born {b}, spouse born {sps}')
        if flag: print(f,c,name(c),'|',';'.join(flag))
print('families with exactly one spouse:',onesp)
