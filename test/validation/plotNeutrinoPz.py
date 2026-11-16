"""Reconstructed vs LHE neutrino p_z for the leptonMET channels of an eecNano_cfg.py MC output (default.yaml).
Usage: python3 plotNeutrinoPz.py nano.root out.png [--wMass 80.3692]
For W -> l nu samples with an LHEEventProduct (e.g. WZtoLNu2Q_powheg): events with one selected lepton and exactly one
LHE neutrino of the channel's flavour (Wmunu: nu_mu, Wenu: nu_e). Writes the figure and prints per-level, per-status
statistics, including how often the chosen root is the one closer to the true p_z (status 1)."""
import argparse
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import uproot

ap = argparse.ArgumentParser()
ap.add_argument('nano')
ap.add_argument('out')
ap.add_argument('--wMass', type=float, default=80.3692)
args = ap.parse_args()

ev = uproot.open(args.nano)['Events'].arrays(library='np')
NU_PDG = {'Wmunu': 14, 'Wenu': 12}
BLUE, ORANGE = '#2a78d6', '#eb6834'   # status 1, status 3 (dataviz reference palette slots 1 and 2)


def trueNeutrinoPz(i, flavour):
    sel = (np.abs(ev['LHEPart_pdgId'][i]) == flavour) & (ev['LHEPart_status'][i] == 1)
    if sel.sum() != 1:
        return None
    return float(ev['LHEPart_pt'][i][sel][0] * np.sinh(ev['LHEPart_eta'][i][sel][0]))


def collect(prefix):
    """rows (status, pzReco, pzTrue, pzOtherRoot) over both channels at one level ('' reco, 'Gen' gen)."""
    rows = []
    for ch, flavour in NU_PDG.items():
        t = prefix + ch
        for i, st in enumerate(ev[t + 'Neutrino_status']):
            if len(st) != 1:
                continue
            true = trueNeutrinoPz(i, flavour)
            if true is None:
                continue
            ptn, etan, phin = ev[t + 'Neutrino_pt'][i][0], ev[t + 'Neutrino_eta'][i][0], ev[t + 'Neutrino_phi'][i][0]
            ptl, etal, phil = ev[t + 'Leptons_pt'][i][0], ev[t + 'Leptons_eta'][i][0], ev[t + 'Leptons_phi'][i][0]
            pz = ptn * np.sinh(etan)
            mu = 0.5 * args.wMass ** 2 + ptl * ptn * np.cos(phin - phil)
            centre = mu * ptl * np.sinh(etal) / ptl ** 2
            rows.append((int(st[0]), pz, true, 2 * centre - pz))
    return np.array(rows).reshape(-1, 4)


fig, axs = plt.subplots(2, 2, figsize=(11, 9), constrained_layout=True)
for col, (prefix, title) in enumerate((('', 'reco'), ('Gen', 'gen (particle level)'))):
    r = collect(prefix)
    ax, axh = axs[0, col], axs[1, col]
    print('--- %s: %d events' % (title, len(r)))
    for st, colour, marker, name in ((1, BLUE, 'o', 'two real roots (status 1)'), (3, ORANGE, '^', 'complex roots (status 3)')):
        m = r[:, 0] == st
        if not m.any():
            continue
        d = r[m, 1] - r[m, 2]
        ax.scatter(r[m, 2], r[m, 1], s=14, marker=marker, color=colour, alpha=0.6, linewidths=0, label='%s, n = %d' % (name, m.sum()))
        axh.hist(np.clip(d, -400, 400), bins=np.linspace(-400, 400, 41), histtype='step', lw=2, color=colour,
                 label='%s: median %.0f, rms %.0f GeV' % (name, np.median(d), d.std()))
        line = '%s status %d: n=%d, median(reco-true)=%.1f GeV, 68%% |reco-true| < %.1f GeV' % (title, st, m.sum(), np.median(d), np.percentile(np.abs(d), 68))
        if st == 1:
            closer = np.abs(r[m, 1] - r[m, 2]) < np.abs(r[m, 3] - r[m, 2])
            line += '; chosen root closer to truth in %.1f%%' % (100 * closer.mean())
        print(line)
    lim = 600
    ax.plot([-lim, lim], [-lim, lim], color='#52514e', lw=1)
    ax.set(xlim=(-lim, lim), ylim=(-lim, lim), xlabel='true (LHE) neutrino $p_z$ [GeV]', ylabel='reconstructed neutrino $p_z$ [GeV]', title=title)
    ax.set_aspect('equal')
    ax.legend(loc='upper left', frameon=False, fontsize=9)
    axh.set(xlabel='reco $-$ true neutrino $p_z$ [GeV] (clipped at $\\pm$400)', ylabel='events')
    axh.legend(loc='upper right', frameon=False, fontsize=9)
    for a in (ax, axh):
        a.grid(color='#e5e4e0', lw=0.6)
        for s in ('top', 'right'):
            a.spines[s].set_visible(False)
fig.savefig(args.out, dpi=130)
print('wrote', args.out)
