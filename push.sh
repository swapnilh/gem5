git push -u multifacet devel/accel:swapnil/devel/accel
# Need to force push as prot-only is rebased often
git push -u multifacet swapnil/devel/prot-only:swapnil/devel/prot-only -f
git push -u multifacet devel/accel-ideal:swapnil/devel/accel-ideal -f
git push origin devel/accel
git push origin swapnil/devel/prot-only -f
git push origin devel/accel-ideal -f
