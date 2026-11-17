import sys
rows=[l.rstrip('\n').split('\t') for l in open(sys.argv[1])]
short=['#','verb','target','verdict','prs','c2p','work','lt','ltmax','cW','cPx','cB','pW','pPx','pWin','tW','below','cBel']
print(' '.join(f'{h:>6}' if i not in (1,2,3) else f'{h:<16}' for i,h in enumerate(short)))
for r in rows[1:]:
    print(' '.join((f'{c[:16]:<16}' if i in (1,2,3) else f'{c:>6}') for i,c in enumerate(r[:18])))
