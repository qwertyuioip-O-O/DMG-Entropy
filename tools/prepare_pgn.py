#!/usr/bin/env python3
"""Prepare an *unseen* standard-initial-position, unannotated SAN bundle from public PGN.

Uses only Python stdlib for text preprocessing, then the project's san_to_uci
for actual legality and UCI conversion. Any failure is explicit, not guessed.
Does not download or train a model. One game per line in output .uci-bundle.
"""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

MOVE_NO = re.compile(r"(?<!\S)\d+\.(?:\.\.)?|(?<!\S)\d+\.\.\.")
RESULT = {'1-0', '0-1', '1/2-1/2', '*'}

def records(text):
    tags, body = {}, []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            if body and any(x.strip() for x in body):
                yield tags, '\n'.join(body)
                tags, body = {}, []
            m = re.match(r'^\[([^ \]]+)\s+"((?:[^"\\]|\\.)*)"\]$', stripped)
            if m: tags[m.group(1)] = m.group(2)
        elif stripped:
            body.append(line)
    if body and any(x.strip() for x in body):
        yield tags, '\n'.join(body)

def strip_comments_and_variations(text):
    result=[]; braces=0; parens=0; semicolon=False
    for c in text:
        if c=='\n': semicolon=False
        if semicolon: continue
        if c==';' and not braces and not parens: semicolon=True; continue
        if c=='{': braces+=1; continue
        if c=='}' and braces: braces-=1; continue
        if braces: continue
        if c=='(': parens+=1; continue
        if c==')' and parens: parens-=1; continue
        if not parens: result.append(c)
    return ''.join(result)

def san_tokens(body):
    clean=strip_comments_and_variations(body)
    clean=re.sub(r'\$\d+', ' ', clean)
    clean=MOVE_NO.sub(' ', clean)
    result=[]
    for raw in clean.split():
        if raw in RESULT: break
        if raw.startswith('%') or raw.startswith('['): continue
        token=re.sub(r'^(\d+\.)+', '', raw)
        if token and token not in RESULT and token!='...':
            if re.match(r'^\d+\.{1,3}$',token): continue
            token=re.sub(r'([!?]+)$','',token)
            if token:result.append(token)
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input',type=Path)
    ap.add_argument('output',type=Path)
    ap.add_argument('--converter',type=Path,required=True)
    ap.add_argument('--limit',type=int,default=250)
    ap.add_argument('--min-plies',type=int,default=40)
    ap.add_argument('--log',type=Path)
    args=ap.parse_args()
    data=args.input.read_bytes()
    text=data.decode('utf-8-sig',errors='replace')
    selected=[];total=0;skips={}
    for headers,body in records(text):
        total+=1
        if len(selected)>=args.limit:break
        variant=headers.get('Variant','Standard').lower()
        if variant not in ('standard','chess') or headers.get('SetUp')=='1' or headers.get('FEN'):
            skips['variant_or_custom_start']=skips.get('variant_or_custom_start',0)+1;continue
        if headers.get('WhiteTitle')=='BOT' or headers.get('BlackTitle')=='BOT':
            skips['bot']=skips.get('bot',0)+1;continue
        toks=san_tokens(body)
        if len(toks)<args.min_plies:
            skips['short_or_unparsed']=skips.get('short_or_unparsed',0)+1;continue
        selected.append(toks)
    if not selected:raise SystemExit('no usable games')
    with tempfile.TemporaryDirectory() as d:
        inp=Path(d)/'multi.san';out=Path(d)/'multi.uci'
        inp.write_text('\n---\n'.join(' '.join(seq) for seq in selected)+'\n',encoding='utf8')
        run=subprocess.run([str(args.converter),str(inp),str(out)],capture_output=True,text=True)
        if run.returncode:
            raise SystemExit('SAN converter rejected input; nothing written. Error: '+run.stderr.strip())
        lines=[line.strip() for line in out.read_text().splitlines() if line.strip()]
    if len(lines)!=len(selected):raise SystemExit(f'game-count mismatch: {len(lines)} != {len(selected)}')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text('\n'.join(lines)+'\n')
    audit={'input':str(args.input),'input_sha256':hashlib.sha256(data).hexdigest(),
           'games_scanned':total,'games_selected':len(selected),'plies':sum(len(line.split()) for line in lines),
           'selection':{'max_games':args.limit,'min_plies':args.min_plies,'variant':'standard','initial':'standard','no_bots':True},
           'skips':skips,'validation':'SAN parsed as legal UCI by project converter',
           'warning':'This converts a supplied PGN. It is not a random sample unless the supplied PGN is sampled appropriately.'}
    log=args.log or args.output.with_suffix('.audit.json')
    log.write_text(json.dumps(audit,indent=2,ensure_ascii=False)+'\n')
    print(json.dumps(audit,ensure_ascii=False))

if __name__=='__main__':main()
