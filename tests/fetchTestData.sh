#!/bin/bash
# Fetch APPLgrids and LHgrids for testing the APFELgrid plugin
lhapdf install CT10
#wget http://www.hepforge.org/archive/applgrid/atlas-Z0-arxiv-1109.5141.tgz
wget http://applgrid.hepforge.org/downloads/atlas-Z0-arxiv-1109.5141.tgz
tar -xvzf ./atlas-Z0-arxiv-1109.5141.tgz --strip-components 2
rm atlas-Z0-arxiv-1109.5141.tgz
mv atlas-Z0-rapidity.root ./tests
