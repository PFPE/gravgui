import tie_functions as tf
from datetime import datetime
import os, sys

####
# script to run for command line gravity tie calc
####

tie = tf.tie(cmd=True)

# TODO add option to load a tie that has already been partially completed:
# have a list of tasks to complete, modify that list according to read-in starting point (ie skip option for land tie, option for heights, option for DGS)

iq = input('set to use metric units for inputs; enter <i> to switch to imperial -> ') or ''
if iq == 'i':
    tie.use_imperial()

# pick a ship
tie.choose_ship()
assert hasattr(tie,'ship'), 'exiting'

# pick a base station
tie.choose_station()
assert hasattr(tie,'station'), 'exiting'

# decide if this is a land tie or not
iq = input('enter <l> if this tie will include a land tie -> ') or ''
if iq == 'l':
    tie.choose_meter()
    assert hasattr(tie,'meter'), 'exiting'
    tie.land_tie_counts()
    assert hasattr(tie,'land_tie_value'), 'land tie unsuccesful, exiting'

    print('saving land tie paramters just in case')
    tie.save_tie(opath='landtie-%s.json' % datetime.today().isoformat())

# for all kinds of ties, collect pier water height measurements
tie.three_water_heights()
assert hasattr(tie,'heights'), 'exiting'
print('checkpoint: saving parameters here')
tie.save_tie(opath='water-heights-%s.json' % datetime.today().isoformat())

# read in dgs data
dgsfile = input('enter path to dgs file: ')
israw = False
iq = input('if this is a raw file, enter <r>; otherwise will assume laptop file -> ')
if iq == 'r': israw = True
tie.calculate_average_meter_grav(dgsfile,raw=israw)

# compute bias
tie.compute_bias()

# write report TODO