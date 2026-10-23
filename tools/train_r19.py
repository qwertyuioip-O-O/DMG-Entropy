#!/usr/bin/env python3
"""Fit fused categorical models on development examples only."""
import argparse,struct,json,hashlib
from pathlib import Path
import numpy as np
from scipy.optimize import minimize
p=argparse.ArgumentParser();p.add_argument('examples');p.add_argument('out');a=p.parse_args();root=Path(__file__).resolve().parents[1]
b=Path(a.examples).read_bytes();pos=0;weights=[];keys=[];response=[];geometry=[];starts=[];chosen=[]
while pos<len(b):
 n,ix=struct.unpack_from('<HH',b,pos);pos+=4
 if not 0<=ix<n<=256:raise ValueError('record')
 starts.append(len(weights));chosen.append(len(weights)+ix)
 for _ in range(n):
  w,*k=struct.unpack_from('<H7H',b,pos);pos+=16
  if not w or max(k[:5])>=672 or max(k[5:])>=48:raise ValueError('candidate')
  weights.append(w);keys.append(k[:5]);response.append(k[5]);geometry.append(k[6])
weights=np.asarray(weights,dtype=float);keys=np.asarray(keys,dtype=np.int32);starts=np.asarray(starts);chosen=np.asarray(chosen);counts=np.diff(np.r_[starts,len(weights)]);rows=np.repeat(np.arange(len(starts)),counts);logw=np.log(weights);N=len(starts);unit=np.log(2)/64;limit=4*np.log(2)
g=np.loadtxt(root/'benchmarks-r17/prior-frozen.txt');c=np.loadtxt(root/'benchmarks-r18/prior-frozen.txt');initial=np.clip(np.tile(g,4)+c,-256,256)*unit
out=Path(a.out);out.mkdir(parents=True,exist_ok=True);report={'example_sha256':hashlib.sha256(b).hexdigest(),'positions':N,'candidates':len(weights),'fits':[]}
for mode in [2,3,4]:
 features=keys if mode==2 else np.column_stack([keys,672+np.asarray(response if mode==3 else geometry,dtype=np.int32)]);size=672 if mode==2 else 720
 for reg in [1e-5,1e-4,1e-3]:
  def objective(beta):
   score=beta[features].sum(axis=1);inside=(score> -limit)&(score<limit);z=logw+np.clip(score,-limit,limit);mx=np.maximum.reduceat(z,starts);e=np.exp(z-mx[rows]);den=np.add.reduceat(e,starts);prob=e/den[rows];loss=np.mean(mx+np.log(den)-z[chosen])+reg*np.dot(beta,beta)/2;prob[chosen]-=1;prob*=inside/N
   grad=sum(np.bincount(features[:,j],weights=prob,minlength=size) for j in range(features.shape[1]))+reg*beta
   return loss,grad
  if mode==2 and reg==1e-5:
   rng=np.random.default_rng(19);beta=rng.normal(0,0.05,size);direction=rng.normal(size=size);direction/=np.linalg.norm(direction);eps=1e-5;numeric=(objective(beta+eps*direction)[0]-objective(beta-eps*direction)[0])/(2*eps);analytic=float(objective(beta)[1]@direction);error=abs(numeric-analytic);assert error<1e-6;report['gradient_directional_error']=error
  start=initial.copy() if mode==2 else np.r_[initial,np.zeros(48)]
  result=minimize(objective,start,jac=True,method='L-BFGS-B',bounds=[(-limit,limit)]*size,options={'maxiter':300,'ftol':1e-11,'gtol':1e-7})
  theta=np.clip(np.rint(result.x/unit),-256,256).astype(int)
  if mode==2:theta=np.r_[theta,np.zeros(48,dtype=int)]
  name=f'm{mode}-reg{reg:g}.txt';(out/name).write_text(' '.join(map(str,theta))+'\n');item={'file':name,'mode':mode,'regularization':reg,'success':bool(result.success),'message':str(result.message),'iterations':int(result.nit),'objective':float(result.fun),'coefficient_min':int(theta.min()),'coefficient_max':int(theta.max()),'sha256':hashlib.sha256((out/name).read_bytes()).hexdigest()};report['fits'].append(item);print(json.dumps(item),flush=True)
(out/'training.json').write_text(json.dumps(report,indent=2)+'\n')
