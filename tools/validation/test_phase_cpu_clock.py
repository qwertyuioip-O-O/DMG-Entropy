#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,tempfile,os,json
p=argparse.ArgumentParser();p.add_argument('--library',required=True);p.add_argument('--out',required=True);a=p.parse_args()
lib=Path(a.library).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
source='''#include <chrono>
#include <time.h>
#include <iostream>
double stamp(clockid_t c){timespec t;clock_gettime(c,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(){auto a=std::chrono::steady_clock::now();double c=stamp(CLOCK_PROCESS_CPUTIME_ID),r=stamp(CLOCK_REALTIME);timespec delay{0,100000000};nanosleep(&delay,nullptr);auto b=std::chrono::steady_clock::now();std::cout<<std::chrono::duration<double>(b-a).count()<<' '<<stamp(CLOCK_PROCESS_CPUTIME_ID)-c<<' '<<stamp(CLOCK_REALTIME)-r<<'\\n';}
'''
with tempfile.TemporaryDirectory() as tmp:
 f=Path(tmp)/'probe.cpp';b=Path(tmp)/'probe';f.write_text(source)
 subprocess.run(['g++','-std=c++20','-O2',str(f),'-o',str(b)],check=True)
 clean=os.environ.copy();clean.pop('LD_PRELOAD',None);pre=clean|{'LD_PRELOAD':str(lib)}
 normal=list(map(float,subprocess.check_output([str(b)],env=clean,text=True).split()))
 cpu=list(map(float,subprocess.check_output([str(b)],env=pre,text=True).split()))
 assert normal[0]>=.07 and normal[2]>=.07,normal
 assert 0<=cpu[0]<.05 and cpu[2]>=.07,cpu
 assert abs(cpu[0]-cpu[1])<.01,cpu
(out/'clock-probe.json').write_text(json.dumps({'probe':'same libstdc++ steady_clock path as frozen CLI','normal':[normal[0],normal[1],normal[2]],'interposed':[cpu[0],cpu[1],cpu[2]],'columns':['chrono_interval','process_cpu_interval','realtime_interval'],'result':'PASS'},indent=2));print('PASS chrono uses process CPU clock under child-only interposer; realtime remains wall time')
