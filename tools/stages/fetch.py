import sys, json, time
import pyarrow.dataset as ds, pyarrow.fs as pfs, pyarrow.compute as pc
from shapely import wkb
from shapely.geometry import mapping
def fetch(theme, kind, lo0, la0, lo1, la1, out):
    fs = pfs.S3FileSystem(anonymous=True, region='us-west-2')
    d = ds.dataset(f'overturemaps-us-west-2/release/2026-11-18.0/theme={theme}/type={kind}/', filesystem=fs, format='parquet')
    f = (pc.field('bbox','xmin') < lo1) & (pc.field('bbox','xmax') > lo0) & (pc.field('bbox','ymin') < la1) & (pc.field('bbox','ymax') > la0)
    t0=time.time()
    cols=[c for c in ['id','geometry','subtype','class','names'] if c in d.schema.names]
    tab = d.to_table(filter=f, columns=cols)
    feats=[]
    for row in tab.to_pylist():
        g=wkb.loads(row['geometry'])
        feats.append({'type':'Feature','properties':{'subtype':row.get('subtype'),'class':row.get('class'),'name':(row.get('names') or {}).get('primary')},'geometry':mapping(g)})
    json.dump({'type':'FeatureCollection','features':feats}, open(out,'w'))
    print(theme, kind, len(feats), round(time.time()-t0,1),'s')
if __name__=='__main__':
    theme, kind=sys.argv[1], sys.argv[2]; b=list(map(float,sys.argv[3:7])); fetch(theme, kind,*b,sys.argv[7])
