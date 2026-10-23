#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];o=a.out.resolve();o.mkdir(parents=True,exist_ok=True)
original=(r/'tools/native/r19_g10index.cpp').read_text();s=original
clock=r'''
struct CohortPhaseClock {
 using Clock=std::chrono::steady_clock;
 std::array<double,3> seconds{};std::array<uint64_t,3> plies{};
 int active=-1;Clock::time_point stamp;
 void enter(unsigned stage){
  ++plies[stage];if(active==int(stage))return;
  auto now=Clock::now();if(active>=0)seconds[active]+=std::chrono::duration<double>(now-stamp).count();active=int(stage);stamp=now;
 }
 void stop(){if(active<0)return;auto now=Clock::now();seconds[active]+=std::chrono::duration<double>(now-stamp).count();active=-1;}
};
static CohortPhaseClock* cohort_phase_clock=nullptr;
'''
changes=[(';auto s=step19',';if(cohort_phase_clock)cohort_phase_clock->enter(stage);auto s=step19',2),('\n }}auto payload','\n }if(cohort_phase_clock)cohort_phase_clock->stop();}auto payload',1),('g.push_back(m);}games.push_back','g.push_back(m);}if(cohort_phase_clock)cohort_phase_clock->stop();games.push_back',1)]
for old,new,count in changes:
 assert s.count(old)==count,(old,s.count(old));s=s.replace(old,new)
proof=s
for old,new,count in reversed(changes):proof=proof.replace(new,old)
assert proof==original
s=s.replace('V6Archive encode19(',clock+'\nV6Archive encode19(',1)
branch=r'''
 if(cmd=="phase"){
  if(argc!=5)fail("phase BUNDLE TABLE REPETITIONS");unsigned reps=std::stoul(argv[4]);if(!reps||reps>20)fail("phase repetitions");
  MidDiagnostics diag;auto reference=encode19(games,3,table,&diag);if(decode19(reference.bytes,table)!=games)fail("phase warmup");
  std::cout<<"iteration,stage,plies,cdf_bits,encode_seconds,decode_seconds,payload_bits,archive_bytes\n"<<std::setprecision(17);
  for(unsigned rep=0;rep<reps;++rep){
   CohortPhaseClock enc,dec;cohort_phase_clock=&enc;auto encoded=encode19(games,3,table);cohort_phase_clock=nullptr;
   if(encoded.bytes!=reference.bytes)fail("phase encoded byte mismatch");
   cohort_phase_clock=&dec;auto decoded=decode19(reference.bytes,table);cohort_phase_clock=nullptr;if(decoded!=games)fail("phase decoded mismatch");
   for(unsigned phase=0;phase<3;++phase){if(enc.plies[phase]!=diag.phase[phase].moves||dec.plies[phase]!=enc.plies[phase])fail("phase count mismatch");
    std::cout<<rep<<','<<phase<<','<<enc.plies[phase]<<','<<diag.phase[phase].logloss<<','<<enc.seconds[phase]<<','<<dec.seconds[phase]<<','<<reference.payload_bits<<','<<reference.bytes.size()<<'\n';
   }
  }return 0;
 }
'''
s=s.replace(' if(cmd=="test")',branch+' if(cmd=="test")',1);src=r/'tools/native/r19_cohort_phase.cpp';src.write_text(s)
f=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include'),'-I'+str(r/'src')]
subprocess.run([*f,str(src),str(r/'src/experiments/goal10/rmc_g10static.cpp'),'-o',str(o/'cohort_phase')],check=True)
(o/'source-proof.json').write_text(json.dumps({'original_wrapper_sha256':hashlib.sha256(original.encode()).hexdigest(),'removing_only_clock_hooks_restores_original_source':True,'clock':'steady_clock wall; timestamps only at phase transitions/game end; per-ply counter/branch remains','exclusions':'per-game setup, file framing/checksum, file input and parsing excluded from phase timers; measured separately by untouched native speed','generated_source_sha256':hashlib.sha256(s.encode()).hexdigest(),'binary_sha256':hashlib.sha256((o/'cohort_phase').read_bytes()).hexdigest()},indent=2))
print(o/'cohort_phase')
