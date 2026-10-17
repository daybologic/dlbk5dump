#!/usr/bin/env python3
"""Independently check GEDCOM syntax, link integrity and family plausibility."""

import re,sys,collections
L=open(sys.argv[1],encoding='utf-8',errors='replace').read().split('\n')
if L and L[-1]=='': L=L[:-1]
recs={};order=[];cur=None;bad=[];prev=-1
ptr=re.compile(r'^@[^@]+@$')
for i,l in enumerate(L,1):
    m=re.match(r'^(\d+) (?:(@[^@]+@) )?(\S+)(?: (.*))?$',l)
    if not m: bad.append((i,'syntax',l));continue
    lv=int(m[1])
    if lv>prev+1: bad.append((i,'level jump',l))
    prev=lv
    if len(l)>255: bad.append((i,'line>255',l[:60]))
    if lv==0:
        cur=[(lv,m[2],m[3],m[4] or '',i)];recs.setdefault(m[2] or m[3],[]).append(cur);order.append(cur)
    else: cur.append((lv,m[2],m[3],m[4] or '',i))
print('bad lines',len(bad));[print(' ',b) for b in bad[:20]]
dups=[k for k,v in recs.items() if len(v)>1 and k.startswith('@')];print('dup xrefs',dups[:10])
types=collections.Counter(r[0][2] for r in order);print(types)
ind={r[0][1]:r for r in order if r[0][2]=='INDI'};fam={r[0][1]:r for r in order if r[0][2]=='FAM'}
def tags(r,t,lv=1):return [x[3] for x in r if x[0]==lv and x[2]==t]
# xref integrity
dang=collections.Counter()
for r in order:
    for x in r[1:]:
        if ptr.match(x[3]) and x[3] not in recs: dang[(x[2],x[3])]+=1
print('dangling pointers',len(dang),list(dang)[:15])
# reciprocity
nr=[]
for f,r in fam.items():
    for t,back in (('HUSB','FAMS'),('WIFE','FAMS'),('CHIL','FAMC')):
        for p in tags(r,t):
            if p in ind and f not in tags(ind[p],back): nr.append((f,t,p))
for p,r in ind.items():
    for t,fw in (('FAMS',('HUSB','WIFE')),('FAMC',('CHIL',))):
        for f in tags(r,t):
            if f in fam and not any(p in tags(fam[f],x) for x in fw): nr.append((p,t,f))
print('non-reciprocal links',len(nr),nr[:15])
# family anomalies
multi=[(f,len(tags(r,'HUSB')),len(tags(r,'WIFE'))) for f,r in fam.items() if len(tags(r,'HUSB'))>1 or len(tags(r,'WIFE'))>1]
print('fams with >1 HUSB/WIFE',len(multi),multi[:15])
sexbad=[]
for f,r in fam.items():
    for t,s in (('HUSB','M'),('WIFE','F')):
        for p in tags(r,t):
            if p in ind and tags(ind[p],'SEX')[:1]!=[s]: sexbad.append((f,t,p,tags(ind[p],'SEX'),tags(ind[p],'NAME')))
print('role/sex mismatch',len(sexbad));[print(' ',x) for x in sexbad[:20]]
emptyf=[f for f,r in fam.items() if not tags(r,'HUSB') and not tags(r,'WIFE')];print('fams without spouses',len(emptyf))
single=[f for f,r in fam.items() if len(tags(r,'HUSB'))+len(tags(r,'WIFE'))+len(tags(r,'CHIL'))<=1];print('fams with <=1 member',len(single))
iso=[p for p,r in ind.items() if not tags(r,'FAMC') and not tags(r,'FAMS')];print('isolated INDI',len(iso),iso[:10])
noname=[p for p,r in ind.items() if not tags(r,'NAME')];print('no NAME',len(noname))
nosurn=[p for p,r in ind.items() if tags(r,'NAME') and '/' not in tags(r,'NAME')[0]];print('NAME without /surname/',len(nosurn),'of',len(ind))
odd=[(p,n) for p,r in ind.items() for n in tags(r,'NAME') if re.search(r'[^A-Za-z .\'\-/,()]',n)];print('names w/ odd chars',len(odd),odd[:25])
famc2=[p for p,r in ind.items() if len(tags(r,'FAMC'))>1];print('INDI with >1 FAMC',len(famc2),famc2[:10])
# dates
months='JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC'.split()
dre=re.compile(r'^(((ABT|BEF|AFT|EST|CAL) )?((\d{1,2} )?('+'|'.join(months)+r') )?\d{3,4}(/\d\d)?|\([^()]*\))$')
baddate=[(x[4],x[3]) for r in order for x in r if x[2]=='DATE' and not dre.match(x[3])]
print('non-standard DATE values',len(baddate),baddate[:25])
ev=collections.Counter(x[2] for r in order for x in r if x[0]==1);print(ev)
nobirt=sum(1 for r in ind.values() if not tags(r,'BIRT'));print('INDI without BIRT',nobirt)
# self-parent / cycle quick check
cyc=[f for f,r in fam.items() if set(tags(r,'CHIL'))&set(tags(r,'HUSB')+tags(r,'WIFE'))];print('fams where spouse is also child',cyc)
# chronology: child born before parent
def year(p):
    r=ind.get(p);
    if not r:return None
    for k,x in enumerate(r):
        if x[2]=='BIRT':
            for y in r[k+1:]:
                if y[0]<=1:break
                if y[2]=='DATE':
                    m=re.search(r'(\d{4})',y[3]);return int(m[1]) if m else None
chron=[]
for f,r in fam.items():
    for par in tags(r,'HUSB')+tags(r,'WIFE'):
        for c in tags(r,'CHIL'):
            a,b=year(par),year(c)
            if a and b and (b-a<13 or b-a>65): chron.append((f,par,a,c,b))
print('implausible parent/child birth gaps',len(chron));[print(' ',x) for x in chron[:30]]
