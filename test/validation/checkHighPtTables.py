"""Compares the high-pT muon tables in python/muonHighPtRun3_cff.py with the MUO POG wiki source (muo-wiki git repo,
docs/guidelines/corrections.md, Run 3 tab): every kappa_b value and uncertainty, sigma(p) coefficient and MC smearing factor.
Usage: python3 checkHighPtTables.py <muo-wiki>/docs/guidelines/corrections.md"""
import re
import sys

from SRothman.EventSelection.muonHighPtRun3_cff import ERAS

text = open(sys.argv[1]).read()
run3 = text[text.index('**the recommendation for Run3 is to correct'):]
ok = True


def check(name, ours, wiki):
    global ok
    good = len(ours) == len(wiki) and all(abs(a - b) < 1e-12 for a, b in zip(ours, wiki))
    ok &= good
    print(('PASS ' if good else 'FAIL ') + name + (': %s vs wiki %s' % (ours, wiki) if not good else ''))


headings = {'2022preEE': '2022 preEE', '2022postEE': '2022 postEE', '2023preBPix': '2023 preBPix',
            '2023postBPix': '2023 postBPix'}
scale, resol = run3[:run3.index('#### High pT: Momentum Resolution')], run3[run3.index('=== "Run 3"', run3.index('#### High pT: Momentum Resolution')):]
num = r'(-?\d+\.\d+)'
for era, h in headings.items():
    block = scale[scale.index('**%s $\\kappa_{b}$' % h):]
    rows = [l for l in block.splitlines() if l.strip().startswith('| $') and 'phi<' in l][:3]
    vals = [re.findall(num + r' \(' + num + r'\)', r) for r in rows]
    check(era + ' kappa', ERAS[era]['kappa'], [float(v) for r in vals for v, _ in r])
    check(era + ' kappaUnc', ERAS[era]['kappaUnc'], [float(u) for r in vals for _, u in r])
    block = resol[resol.index('**%s**' % h):]
    block = block[:block.index('<span', 10)] if '<span' in block[10:] else block
    sig = re.findall(r'\\sigma\(p, (?:\\mid\\eta\\mid <1\.2|1\.2<\\mid\\eta\\mid <2\.4)\) = (.*?)\$', block)
    coeffs = []
    for s in sig:
        s = s.replace(' ', '')
        c0 = float(re.match(r'(-?[\d.]+)', s).group(1))
        rest = [float(a) * 10 ** float(b) for a, b in re.findall(r'([\d.]+)\\times10\^\{(-?\d+)\}', s)]
        signs = re.findall(r'([+-])[\d.]+\\times', s)
        coeffs.append([c0] + [v * (-1 if sg == '-' else 1) for v, sg in zip(rest, signs)])
    check(era + ' sigmaBarrel', ERAS[era]['sigmaBarrel'], coeffs[0])
    check(era + ' sigmaEndcap', ERAS[era]['sigmaEndcap'], coeffs[1])
    central = 'no extra smearing to the MC needs to be done for them' in block
    factors = [float(f) for f in re.findall(r'\\times (\d\.\d+)\)\)', block)]
    m = re.search(r'\\times (\d\.\d+)\((\d\.\d+)\)\)', block)
    wiki = (0.0, factors[0]) if central else (float(m.group(1)), float(m.group(2)))
    check(era + ' smearing factors', [ERAS[era]['smearBarrel'], ERAS[era]['smearEndcap']], list(wiki))
sys.exit(0 if ok else 1)
