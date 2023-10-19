import numpy as np
from scipy.signal import firwin, filtfilt
from datetime import datetime, timezone
from copy import copy
import json
import os

####
# tie class for holding tie info and calculating ties and all of that
# also some functions for loading DGS meter data files
####

# some useful constants
faafactor = 0.3086;
GravCal = 414125;
otherfactor = 8388607
g0 = 10000;

# relative paths to databae files
landmeters_db = 'database/landmeters.db'
ships_db = 'database/ships.db'
stations_db = 'database/stations.db'
land_cal_dir = 'database/land-cal/'

class tie:
    def __init__(self,cmd=True,units='metric'):
        """ Initialize tie class.
        Lists of ships, stations, and meters in database are read in when object initializes.
        cmd flag sets whether methods are command-line interactive or not (GUI).
        We assume tie is not a land tie unless land_meter_counts() method is called later.
        """
        with open(ships_db,'r') as f:
            shipdat = f.read().split('\n')
            shipdat = [line.split('=')[-1].strip("\"") for line in shipdat if line.startswith('SHIP')]
            self.shipdat = shipdat
        with open(stations_db,'r') as f:
            stadat = f.read().split('\n')
            station_numbers = [line.split('_')[-1][:-1] for line in stadat if line.startswith("[STATION")]
            station_names = [line.split('=')[-1].strip("\"") for line in stadat if line.startswith("NAME")]
            self.stadat = [(station_numbers[e],station_names[e]) for e in range(len(station_numbers))]
        with open(landmeters_db,'r') as f:
            landdat = f.read().split('\n')
            meter_numbers = [line.split('_')[-1][:-1] for line in landdat if line.startswith("[LAND_METER")]
            meter_names = [line.split('=')[-1].strip("\"") for line in landdat if line.startswith("SN")]
            self.landdat = [(meter_numbers[e],meter_names[e]) for e in range(len(meter_numbers))]

        self.cmd = cmd  # setting for whether this is a command line tie or a GUI tie
        self.landtie = False  # assume this will not include a land tie unless told otherwise
        self.units = units  # metric or imperial, for pier height entries; default is metric
        return

    def use_imperial(self):
        self.units = 'imperial'
        return

    def imp_to_metric(self,feet,inches):
        """convert ft+inches measurement to meters"""
        ft_dec = feet + inches/12
        meters = ft_dec/3.281
        return meters

    def save_tie(self,opath=None):
        """ Seralize whatever info we have at the moment, for re-reading later (not pretty report).
        This is a workaround because ordinary json doesn't like custom classes or datetimes.
        """
        output = {'cmd':self.cmd,'landtie':self.landtie}
        if hasattr(self,'ship'):
            output['ship'] = self.ship
        if hasattr(self,'station'):
            output['station'] = self.station
        if hasattr(self,'secondary_station_name'):
            output['secondary_station_name'] = self.secondary_station_name
        if hasattr(self,'meter'):
            output['meter'] = self.meter
        if hasattr(self,'heights'):
            oh = copy(self.heights)
            for k in oh.keys():
                oh[k] = (oh[k][0],oh[k][1].isoformat())
            output['heights'] = oh
        if hasattr(self,'meter_counts'):
            omc = copy(self.meter_counts)
            for k in omc.keys():
                omc[k] = (omc[k][0],[e.isoformat() for e in omc[k][1]],omc[k][2])
            output['meter_counts'] = omc
        if hasattr(self,'land_tie_value'):
            output['land_tie_value'] = self.land_tie_value
        if hasattr(self,'drift'):
            output['drift'] = self.drift
        if hasattr(self,'avg_dgs_grav'):
            output['avg_dgs_grav'] = self.avg_dgs_grav
        if hasattr(self,'water_grav'):
            output['water_grav'] = self.water_grav
        if hasattr(self,'bias'):
            output['bias'] = self.bias

        if opath==None:
            opath = self.ship.split(' ')[-1] + datetime.today().isoformat() + '.json'
        if self.cmd: print('writing tie parameters to %s' % opath)
        fo = open(opath,'w')
        json.dump(output,fo)
        fo.close()
        return
    
    def reread_tie(self,ifile):
        """ Re-read json file with dict of previous tie info and (over)write into the tie object where this method is called.
        Useful if you want to take pier measurements, save, and come back later to upload DGS data and do the bias calc.
        """
        fo = open(ifile,'r')
        itie = json.load(fo)
        fo.close()
        # reset all the things!
        if 'cmd' in itie.keys():
            self.cmd = itie['cmd']
        if 'landtie' in itie.keys():
            self.landtie = itie['landtie']
        if 'ship' in itie.keys():
            self.ship = itie['ship']
        if 'station' in itie.keys():
            self.station = tuple(itie['station'])
            self._read_station_info()
        if 'secondary_station_name' in itie.keys():
            self.secondary_station_name = itie['secondary_station_name']
        if 'meter' in itie.keys():
            self.meter = tuple(itie['meter'])
            self._read_meter_cal_table()
        if 'heights' in itie.keys():
            self.heights = itie['heights']
            for k in self.heights.keys():
                self.heights[k] = (self.heights[k][0],datetime.fromisoformat(self.heights[k][1]))
        if 'meter_counts' in itie.keys():
            self.meter_counts = itie['meter_counts']
            for k in self.meter_counts.keys():
                self.meter_counts[k] = (tuple(self.meter_counts[k][0]),tuple([datetime.fromisoformat(e) for e in self.meter_counts[k][1]]),tuple(self.meter_counts[k][2]))
        if 'land_tie_value' in itie.keys():
            self.land_tie_value = itie['land_tie_value']
        if 'drift' in itie.keys():
            self.drift = itie['drift']
        if 'avg_dgs_grav' in itie.keys():
            self.avg_dgs_grav = itie['avg_dgs_grav']
        if 'water_grav' in itie.keys():
            self.water_grav = itie['water_grav']
        if 'bias' in itie.keys():
            self.bias = itie['bias']
        return

    def choose_ship(self,**kwargs):
        """ Select which ship this tie is for. Ship is used only for writing out report.
        """
        if not self.cmd:  # this is the gui option
            assert 'ship' in kwargs.keys(), 'no ship name supplied'
            ship = kwargs['ship']

        if self.cmd:
            print('ship options:')
            for i,s in enumerate(self.shipdat):
                print('%3i   %s' % (i,s))
            iq = int(input('select a ship by its number -> ') or -1)
            try:
                ship = self.shipdat[iq]
            except IndexError:
                print('invalid choice; no ship selected')
                return
        self.ship = ship
        return

    def choose_station(self,**kwargs):
        """Select reference base station for this tie. This is used to read the absolute
        gravity measurement from the station database file (unless station is not in the database
        or has no assigned gravity in the database, in which case user supplies gravity value)"""
        if not self.cmd:  # supplied from gui
            assert 'station' in kwargs.keys(), 'no station name supplied'
            station = kwargs['station']
        if self.cmd:
            print('station options:')
            for i,s in enumerate(self.stadat):
                print('%3i   %s' % (i,s[1]))
            iq = int(input('select a station by index -> ') or -1)
            try:
                station = self.stadat[iq]
            except IndexError:
                print('invalid choice; no station selected')
                return
        self.station = station
        if self.station[1] == 'Other':
            self.secondary_station_name = input('give this station a descriptive name: ') or 'no name given'
        msg = self._read_station_info()
        if msg == 'missing absolute gravity value':
            print(msg)
            ig = input('enter a value-> ')
            self.station_info['GRAVITY'] = ig
        elif msg == 'station not found in database':  # note that "other" is an option
            print(msg)
            print('station not assigned')  # will need to try again
            del self.station
        return

    def _read_station_info(self):
        """Read metadata on selected station from the database. Returns descriptive error messages
        for stations not found in database, or stations missing absolute gravity measurements, that
        are then handled in the calling function.
        """
        msg = ''
        f = open(stations_db,'r')
        nln = sum(1 for line in f)
        f.seek(0)
        found_sta = False
        for i in range(nln):
            line = f.readline()
            if line.startswith("[STATION_%i]" % int(self.station[0])):
                found_sta = True
                break
        if not found_sta:
            msg = 'station not found in database'
            return msg
        
        station_info = {}
        keep_going = True
        while keep_going:
            line = f.readline()
            if '=' in line:
                line = line.split('=')
                if line[1] == '\n': line[1] = None
                station_info[line[0]] = line[1].strip().strip("\"")
            else:
                keep_going = False

        self.station_info = station_info
        try:  # make sure that there is an absolute gravity value available for later
            float(self.station_info['GRAVITY'])
        except ValueError:  # TODO cmd/gui
            msg = 'missing absolute gravity value'
        except KeyError:
            msg = 'missing absolute gravity value'
        return msg

    def choose_meter(self,**kwargs):
        if not self.cmd:  # supplied from gui
            assert 'meter' in kwargs.keys(), 'no land meter name supplied'
            meter = kwargs['meter']
        if self.cmd:
            print('land meter options:')
            for i,s in enumerate(self.landdat):
                print('%3i   %s' % (i,s[1]))
            iq = int(input('select a meter by index -> ') or -1)
            try:
                meter = self.landdat[iq]
            except ValueError:
                print('invalid choice; no meter assigned')
                return
        self.meter = meter
        msg = self._read_meter_cal_table()
        if self.cmd and msg != '':
            print(msg)
        return

    def _read_meter_cal_table(self):
        msg = ''
        if not hasattr(self,'meter'):
            msg = 'no meter name entered, cannot read calibration table'
            return msg
        cal_file = os.path.join(land_cal_dir,self.meter[1]+'.CAL')
        if not os.path.isfile(cal_file):
            msg = 'calibration file %s is missing' % cal_file
            return msg
        cal_table = np.loadtxt(cal_file,skiprows=1)
        self.cal_table = cal_table
        return msg

    def _get_timestamped_float(self,text='value-> '):
        """helper function for getting a float from command line
        with loop checks to make sure there actually is a value"""
        val = np.nan
        idone = False
        while not idone:
            try:
                val = float(input(text)) or np.nan
                ts = datetime.now(tz=timezone.utc)
                idone = True
            except ValueError:
                iq = input('value is NaN. enter <s> to skip this measurement, or <r> to re-try entry -> ') or 'r'
                if iq == 'r':
                    idone = False
                elif iq == 's':
                    ts = datetime.now(tz=timezone.utc)
                    idone = True
        return (val,ts)
                
    def three_water_heights(self,**kwargs):
        """collect three measurements of water height at the pier.
        it is possible to not enter all three measurements; non-measurements are logged as NaN
        """
        if not self.cmd:  # supplied from GUI
            assert 'heights' in kwargs.keys(), 'no heights passed to function'
            heights = kwargs['heights']

        else:
            print('Enter three measurements as they are taken. Timestamps will be logged automatically.')
            if self.units == 'imperial':
                print('Units are imperial; each prompt will ask first for feet and then for inches. Enter 0 for values of 0.')
            heights = {}
            for i in range(3):
                if self.units == 'metric':
                    hval = self._get_timestamped_float(text='water height measurement %i: ' % (i+1))
                    if np.isfinite(hval[0]):
                        heights['H%i' % (i+1)] = hval
                elif self.units == 'imperial':
                    hvft = self._get_timestamped_float(text='meas. %i, FEET: ' % (i+1))
                    hvin = self._get_timestamped_float(text='meas. %i, INCHES: ' % (i+1))
                    if np.isfinite(hvft[0]) and np.isfinite(hvin[0]):
                        met = self.imp_to_metric(feet=hvft[0],inches=hvin[0])
                        heights['H%i' % (i+1)] = (met,hvft[1])  # use feet timestamp
            if len(heights) == 0:
                print('no valid height measurements given; exiting')
                return
        self.heights = heights
        return

    def land_tie_counts(self,**kwargs):
        if not self.cmd:  # supplied from GUI
            if not hasattr(self,'cal_table') or not hasattr(self,'meter') or not 'meter_counts' in kwargs.keys():
                msg = 'missing meter, calibration table, or counts; no tie computed'
                return msg
            meter_counts = kwargs['meter_counts']

        else:
            if not hasattr(self,'cal_table') or not hasattr(self,'meter'):
                print('you must choose a meter and load its calibration table before doing a land tie')
                return
            print('Time to start a land tie! First, enter three successive measurements from the meter taken on the ship. Timestamps will be logged autmomatically.')
            A1vals = []
            for i in range(3):
                A1v = self._get_timestamped_float(text='Measurement %i: ' % (i+1))
                if np.isfinite(A1v[0]):
                    A1vals.append(A1v)

            print('Next, enter three measurements from the meter taken at a known base station. Timestamps will be logged automatically.')
            B1vals = []
            for i in range(3):
                B1v = self._get_timestamped_float(text='Measurement %i: ' % (i+1))
                if np.isfinite(B1v[0]):
                    B1vals.append(B1v)
            
            print('Finally, enter three more measurements from the meter taken on the ship. Timestamps will be logged automatically.')
            A2vals = []
            for i in range(3):
                A2v = self._get_timestamped_float(text='Measurement %i: ' % (i+1))
                if np.isfinite(A2v[0]):
                    A2vals.append(A2v)

            if len(A1vals) == 0 or len(B1vals) == 0 or len(A2vals) == 0:
                print('not enough measurements to do a tie; exiting')
                return
        
            meter_counts = {}
            av = [A1vals,B1vals,A2vals]; lv = ['A1','B1','A2']
            for i in range(3):
                meter_counts[lv[i]] = (tuple([av[i][j][0] for j in range(len(av[i]))]),tuple([av[i][j][1] for j in range(len(av[i]))]))

        self.meter_counts = meter_counts
        self._convert_meter_counts_to_mgal()
        self.landtie = True
        self._compute_land_tie()
        return

    def _check_cal_table(self):  # TODO TODO
        """ make sure meter cal table exists and covers the span of counts"""
        return

    def _convert_meter_counts_to_mgal(self):
        if len(self.cal_table) == 1:
            print('not set up to deal with single-value cal yet, sorry')
            # TODO TODO
            return

        meter_counts = self.meter_counts
        brackets = self.cal_table[:,0]
        for k in meter_counts.keys():
            counts,timestamps = meter_counts[k]
            gravs = []
            for c in counts: # for each individual measurement in the 3 sets of readings:
                try:  # find the read bracket lower end
                    below = brackets[brackets<c].max()
                except ValueError:  # table is incomplete, does not bracket values
                    print('calibration table for meter is not complete')
                    return
                if abs(c-below) > 100:  # not close enough, we're off the edge of the table
                    print('calibration table is incomplete')
                    return
                # ok, if we made it this far, the bracket index is where "below" is
                ind = np.where(brackets==below)[0][0]
                factor = self.cal_table[ind,2]
                offset = self.cal_table[ind,1]
                readbrack = self.cal_table[ind,0]
                residual_reading = c - readbrack
                residual = residual_reading*factor
                gravs.append(offset+residual)

            meter_counts[k] = (counts,timestamps,tuple(gravs))  # reset meter_counts to include calibrated values
        self.meter_counts = meter_counts  # save this in tie if not already linked by oop
        return


    def _compute_land_tie(self):
        """ Take meter readings converted to mGal, with timestamps, and
        compute the land tie value to be used for the bias calculation
        """

        avg_time_A1 = np.mean([t.timestamp() for t in self.meter_counts['A1'][1]])  # this will be in seconds
        avg_time_B1 = np.mean([t.timestamp() for t in self.meter_counts['B1'][1]])
        avg_time_A2 = np.mean([t.timestamp() for t in self.meter_counts['A2'][1]])

        avg_mgals_A1 = np.mean(self.meter_counts['A1'][2])
        avg_mgals_B1 = np.mean(self.meter_counts['B1'][2])
        avg_mgals_A2 = np.mean(self.meter_counts['A2'][2])

        # calculate the drift
        AA_timedelta = avg_time_A2 - avg_time_A1
        AB_timedelta = avg_time_B1 - avg_time_A1
        drift = (avg_mgals_A2-avg_mgals_A1)/AA_timedelta

        dc_avg_mgals_B1 = avg_mgals_B1 - AB_timedelta*drift
        dc_avg_mgals_A = avg_mgals_A1
        reference_g = float(self.station_info['GRAVITY'])

        gdiff = dc_avg_mgals_A - dc_avg_mgals_B1
        grav_A = reference_g + gdiff

        self.land_tie_value = grav_A
        self.drift = drift
        # NOTE could also save all these avg values and times etc? am not bothering to do so right now

        return

    def calculate_average_meter_grav(self,dgsfile,raw=False):
        """ Read in gravimeter file(s), filter and trim to time of pier water measurements, and take avg.
        The dgsfile can be one file path or a list of paths.
        """

        # TODO check for all necessary info, and that dgsfile exists
        if raw:
            dgrav,dates = read_raw_dgs_theor(dgsfile,self.ship)
            rgrav = (dgrav*GravCal/otherfactor) + g0
        else:
            rgrav, dates = read_dat_dgs(dgsfile,self.ship)

        htimes = [self.heights[k][1] for k in self.heights.keys()]
        if min(htimes) < min(dates) or max(htimes) > max(dates):
            print('dgs files do not cover all of the time window for pier height measurements')
            return
        good_inds = np.where(np.logical_and(dates >= min(htimes),dates <= max(htimes)))[0]
        rgrav = rgrav[good_inds]
        dates = dates[good_inds]
        # TODO trim to match pier height readings time window

        ndata = len(rgrav)
        filtertime = int(np.round(ndata/10))
        sampling = 1
        taps = 2*filtertime
        freq = 1/filtertime
        nyquist = sampling/2
        wn = freq/nyquist
        B = firwin(taps,wn,window='blackman')
        fdat = filtfilt(B,1,rgrav)

        self.avg_dgs_grav = np.mean(fdat)

        # TODO plot the filtered gravity if cmd? or only in GUI?

        return

    def compute_bias(self):
        """ Do the actual bias calculation with a tie!
        """
        # TODO check to make sure we have all the pieces we need already

        # get pier heights, make sure they are negative values if not so entered
        pier_heights = [-1*abs(float(self.heights[k][0])) for k in self.heights.keys()]
        avg_pier_height = np.mean(pier_heights)
        if self.landtie:
            pier_grav = self.land_tie_value
        else:
            pier_grav = float(self.station_info['GRAVITY'])

        water_grav = pier_grav + faafactor*avg_pier_height
        bias = water_grav - self.avg_dgs_grav

        self.bias = bias
        self.water_grav = water_grav

        return



########################################################################
#
########################################################################
def read_dat_dgs(fp, ship):
    """ read DGS dat file(s) from a list of paths
    """
    if type(fp) == str:
        fp = [fp,]  # listify
    rgrav_all = np.array([]); dates_all = []
    for path in fp:
        if ship in ['R/V Atlantis','R/V Revelle','R/V Palmer','R/V Ride']:
            rgrav,year,month,day,hour,minute,second = np.loadtxt(path,delimiter=',',usecols=[1,19,20,21,22,23,24],unpack=True,dtype={'names':('a','b','c','d','e','f','g'),'formats':('f4','i4','i4','i4','i4','i4','i4')})
            dates = [datetime(year[i],month[i],day[i],hour[i],minute[i],second[i],tzinfo=timezone.utc) for i in range(len(year))]
        elif ship == 'R/V Thompson':
            date,time,rgrav = np.loadtxt(path,delimiter=',',usecols=[0,1,3],dtype={'names':('a','b','c'),'formats':('S10','S12','f4')},unpack=True)
            datetimes = [date[i]+'-'+time[i] for i in range(len(date))]
            dates = [datetime.strptime(dt,'%m/%d/%Y-%H:%M:%S').replace(tzinfo=timezone.utc) for dt in datetimes]

        rgrav_all = np.append(rgrav_all,rgrav)
        dates_all.append(dates)
    return rgrav_all, np.array(dates_all).T

def read_raw_dgs_theor(fp, ship):
    """ Read in raw serial outputs from DGS AT1M from a list of
    paths. These will be in AD units mostly.
    Formatting here is assumed to follow what the DGS documentation
    says, though some things may vary by vessel?"""

    if type(fp) == str:
        fp = [fp,]  # listify
    dgrav_all = np.array([]); dates_all = []
    for ip, path in enumerate(fp):
        strings,dgrav,timestamp = np.loadtxt(path,delimiter=',',unpack=True,usecols=(0,1,18),dtype={'names':('a','b','c'),'formats':('S10','f4','i4')})

        if ip == 0: # check if [18] is timestamps or seconds counter
            conv_times = True
            if str(timestamp[0]).startswith('1'):  # 1 Hz, 1 second
                conv_times = False # so don't try to convert to stamp
        if conv_times:  # probably UTC stamps
            timestamp = [datetime.strptime(dt,'%Y%m%d%H%M%S').replace(tzinfo=timezone.utc) for dt in timestamp]

        if ship == 'R/V Revelle' and not conv_times and not strings[0].startswith('$'): # not synced for timestamp, but might be in string
            # split string for possible timestamp
            times = [e.split(' ')[0] for e in strings]
            timestamp = [datetime.fromisoformat(dt) for dt in times]

        dgrav_all = np.append(dgrav_all,dgrav)
        dates_all.append(timestamp)

    return dgrav_all, np.array(timestamp).T
