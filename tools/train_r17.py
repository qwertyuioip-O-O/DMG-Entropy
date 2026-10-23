#!/usr/bin/env python3
"""Fit a frozen 168-coefficient middle-game prior using development data only."""
import argparse,struct,json,hashlib
from pathlib import Path
import numpy as np
from scipy.optimize import minimize
ap=argparse.ArgumentParser();ap.add_argument('examples');ap.add_argument('out');a=ap.parse_args()
b=Path(a.examples).read_bytes();pos=0;weights=[];keys=[];starts=[];chosen=[]
while pos<len(b):
 n,ix=struct.unpack_from('<HH',b,pos);pos+=4
 if not 0<=ix<n<=256:raise ValueError('record')
 starts.append(len(weights));chosen.append(len(weights)+ix)
 for _ in range(n):
  w,*k=struct.unpack_from('<H5B',b,pos);pos+=7
  if not w or max(k)>=168:raise ValueError('candidate')
  weights.append(w);keys.append(k)
weights=np.asarray(weights,dtype=np.float64);keys=np.asarray(keys,dtype=np.int32);starts=np.asarray(starts);chosen=np.asarray(chosen);counts=np.diff(np.r_[starts,len(weights)]);rows=np.repeat(np.arange(len(starts)),counts);logw=np.log(weights);N=len(starts);limit=np.log(4);unit=np.log(2)/64
out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
report={'example_sha256':hashlib.sha256(b).hexdigest(),'positions':N,'candidates':len(weights),'fits':[]}
for reg in [1e-5,1e-4,1e-3]:
 def objective(beta):
  score=beta[keys].sum(axis=1);inside=(score> -limit)&(score<limit);z=logw+np.clip(score,-limit,limit);mx=np.maximum.reduceat(z,starts);e=np.exp(z-mx[rows]);den=np.add.reduceat(e,starts);prob=e/den[rows]
  loss=np.mean(mx+np.log(den)-z[chosen])+reg*np.dot(beta,beta)/2
  prob[chosen]-=1;prob*=inside/N
  grad=sum(np.bincount(keys[:,j],weights=prob,minlength=168) for j in range(5))+reg*beta
  return loss,grad
 r=minimize(objective,np.zeros(168),jac=True,method='L-BFGS-B',bounds=[(-limit,limit)]*168,options={'maxiter':200,'ftol':1e-11,'gtol':1e-7})
 theta=np.clip(np.rint(r.x/unit),-128,128).astype(int);name=f'prior-{reg:g}.txt';(out/name).write_text(' '.join(map(str,theta))+'\n')
 item={'file':name,'regularization':reg,'success':bool(r.success),'message':str(r.message),'iterations':int(r.nit),'objective':float(r.fun),'coefficient_min':int(theta.min()),'coefficient_max':int(theta.max()),'sha256':hashlib.sha256((out/name).read_bytes()).hexdigest()};report['fits'].append(item);print(json.dumps(item),flush=True)
(out/'training.json').write_text(json.dumps(report,indent=2)+'\n')
