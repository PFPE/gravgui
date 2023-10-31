#include <gtk/gtk.h>
#include <iomanip>
#include <sstream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <numeric>
#include "tie_structs.h"
#include "grav-constants.h"
#include "filt.h"

void on_compute_bias(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);  // we def need the whole tie for this one
    std::vector<float> heights;  // get all heights and timestamps for water grav calc
    std::vector<time_t> height_stamps;
    double water_grav=-999;
    double avg_dgs_grav=-99999;
    double pier_grav;
    double avg_height=-999;

    if (debug_dgs && gravtie->shinfo.gravgrav.size() == 0) return;

    if (gravtie->heights[0].h1 != -999) {
        heights.push_back(-1*std::abs(gravtie->heights[0].h1));  // all heights should be negative numbers
        if (debug_dgs) {
            height_stamps.push_back(gravtie->shinfo.gravtime[2000]); // kludge for timestamps from file
        } else {
            height_stamps.push_back(gravtie->heights[0].t1);
        }
    }
    if (gravtie->heights[1].h1 != -999) {
        heights.push_back(-1*std::abs(gravtie->heights[1].h1));
        if (debug_dgs) {
            height_stamps.push_back(gravtie->shinfo.gravtime[3000]);
        } else {
            height_stamps.push_back(gravtie->heights[1].t1);
        }
    }
    if (gravtie->heights[2].h1 != -999) {
        heights.push_back(-1*std::abs(gravtie->heights[2].h1));
        if (debug_dgs) {
            height_stamps.push_back(gravtie->shinfo.gravtime[4000]);
        } else {
            height_stamps.push_back(gravtie->heights[2].t1);
        }
    }
    // check for land tie if we are using one
    if (gravtie->lminfo.landtie && gravtie->lminfo.land_tie_value > 0) { // bool, and have a value
        pier_grav = gravtie->lminfo.land_tie_value;
    } else {
        pier_grav = gravtie->stinfo.station_gravity;
    }
    if (heights.size() > 0) { // make sure there is at least one height measurement provided
        if (pier_grav > 0) {// check to make sure we have an absolute grav value
            auto const count = static_cast<float>(heights.size());
            avg_height = std::accumulate(heights.begin(), heights.end(), 0.0) / count;
            water_grav = pier_grav + faafactor*avg_height;
        }
    } else {
        return;  // if no heights, no point in trying to calc things
    }
    // check to make sure we have DGS data *and* it covers the heights time window
    if (gravtie->shinfo.gravgrav.size() > 0) { // we have some meter data
        // sort the gravity values and timestamps from whatever files were read into shinfo
        std::vector<time_t> gtime = gravtie->shinfo.gravtime;
        std::vector<size_t> indices(gtime.size());  // vector of indices for sorting
        std::iota(indices.begin(), indices.end(), 0);
        // Sort indices based on timestamps
        std::sort(indices.begin(), indices.end(), [&gtime](size_t i, size_t j) {
            return gtime[i] < gtime[j];
        });
        // Use sorted indices to reorder the grav and time vectors
        std::vector<float> sortedgrav;
        std::vector<time_t> sortedtime;
        for (size_t i = 0; i < indices.size(); i++) {
            sortedgrav.push_back(gravtie->shinfo.gravgrav[indices[i]]);
            sortedtime.push_back(gtime[indices[i]]);
        }

        time_t result1 = *std::min_element(sortedtime.begin(), sortedtime.end());
        time_t result2 = *std::min_element(height_stamps.begin(), height_stamps.end());

        time_t result3 = *std::max_element(sortedtime.begin(), sortedtime.end());
        time_t result4 = *std::max_element(height_stamps.begin(), height_stamps.end());

        // now that things are sorted, make sure gravtime covers heights timespan
        if (result1 < result2 && result3 > result4) {
            //std::cout << "data coverage!" << std::endl;

            // with sufficient data coverage, apply a filter to the whole grav time series
            // set filter parameters based on length of *sliced* grav around heights times
            auto start = std::lower_bound(sortedtime.begin(), sortedtime.end(), result2);
            auto end = std::upper_bound(sortedtime.begin(), sortedtime.end(), result4);
            int lower_index = std::distance(sortedtime.begin(), start);
            int upper_index = std::distance(sortedtime.begin(), end);
            int ntaps = std::round((upper_index - lower_index)/10);
            // design Blackman window with ntaps based on eventual length of data
            Filter *blackman_filt;
            double out_val;
            blackman_filt = new Filter(Blackman, ntaps, 1, 0.1, 0.2); // only name and ntaps matter, TODO
            // apply filter with zero padding
            std::vector<double> firstpass;
            for (int i=0; i<ntaps; i++) {  // zero-pad front
                out_val = blackman_filt->do_sample( (double) 0);
                firstpass.push_back(out_val);
            }
            for (int i=0; i<sortedgrav.size(); i++) {
                out_val = blackman_filt->do_sample( (double) sortedgrav[i]);
                firstpass.push_back(out_val);
            }
            for (int i=0; i<ntaps; i++) {  // zero-pad end
                out_val = blackman_filt->do_sample( (double) 0);
                firstpass.push_back(out_val);
            }
            // trim firstpass and repeat for secondpass
            std::vector<double> firsttrim(firstpass.begin()+ntaps, firstpass.begin()+ntaps+sortedgrav.size());
            std::vector<double> secondpass;
            for (int i=0; i<ntaps; i++) {  // zero-pad front
                out_val = blackman_filt->do_sample( (double) 0);
                secondpass.push_back(out_val);
            }
            for (int i=0; i<sortedgrav.size(); i++) {
                out_val = blackman_filt->do_sample( (double) firsttrim[i]);
                secondpass.push_back(out_val);
            }
            for (int i=0; i<ntaps; i++) {  // zero-pad end
                out_val = blackman_filt->do_sample( (double) 0);
                secondpass.push_back(out_val);
            }
            // trim secondpass padding and reverse the vector
            std::vector<double> secondtrim(secondpass.begin()+ntaps, secondpass.begin()+ntaps+firsttrim.size());
            std::reverse(secondtrim.begin(), secondtrim.end());

            // now from filtered series, get the slice of grav data that we will average here
            // (indices were caluclated before to figure out ntaps)
            std::vector<double> result(secondtrim.begin() + lower_index, secondtrim.begin() + upper_index);

            // average the filtered and sliced data to get the avg gravity for the bias calc
            double gravsum = 0.0;
            for (const double& value : result) {
                gravsum += value;
            }
            avg_dgs_grav = gravsum/result.size();

        } else {  // TODO hightlight something? DGS load button?
            //std::cout << "no data coverage!" << std::endl;
        }
    } else {
        return;  // if no grav data loaded, can't do anything else
    }
    // calculate! put value in a label so it is visible!
    double bias = water_grav - avg_dgs_grav;
    gravtie->bias = bias;
    gravtie->avg_dgs_grav = avg_dgs_grav;
    gravtie->water_grav = water_grav;
    gravtie->avg_height = avg_height;

    std::stringstream strm;
    strm << std::fixed << std::setprecision(2) << bias;
    std::string test = strm.str();
    char bstring[test.length()+16];
    sprintf(bstring,"Computed bias: %.2f", bias);
    gtk_label_set_text(GTK_LABEL(gravtie->bias_label), bstring);
}

// convert counts to mgals for one timestamped thing and given calibration table
double convert_counts_mgals (val_time c1, calibration calib) {
    double mgals = 0.;
    int cind = 0;
    double min_diff = std::abs(c1.h1 - calib.brackets[cind]);

    for (int i=1; i<calib.brackets.size(); i++) {
        double diff = std::abs(c1.h1 - calib.brackets[i]);
        if (diff < min_diff && calib.brackets[i] <= c1.h1) {
            cind = i;
            min_diff = diff;
        }  // TODO catch if we fall off the end of the table
    }
    double residual_reading = c1.h1 - calib.brackets[cind];  // subtract bracket from counts
    mgals = residual_reading*calib.factors[cind] + calib.mgvals[cind]; // calibrate to mgals!
    return mgals;
}

void on_compute_landtie(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);  // we def need the whole tie for this one

    // first check if we have a calibration table loaded
    if (gravtie->lminfo.calib.brackets.size() == 0) {  // no calibration table read
        return; // we can't do counts conversion so no point here
    }
    float ref_g = gravtie->stinfo.station_gravity;
    if (ref_g < 0) {
        return;  // no station gravity to reference to -> nothing to tie our land tie to
    }

    // for each set of counts measurements, check for values, convert to mgals, and get avgs
    std::vector<double> mgal_averages;
    std::vector<time_t> t_averages;

    // un-vectorize this a bit bc otherwise we can't save mgal values to tie
    // two layers of vectors was too much for referencing for some reason I do not understand
    double mgal_sum = 0;
    time_t t_sum = 0;
    int icounts = 0;
    for (val_time& thisone : gravtie->acounts) {
        if (thisone.h1 > 0) {
            double mgalval = convert_counts_mgals(thisone,gravtie->lminfo.calib);
            thisone.m1 = mgalval;
            mgal_sum += mgalval;
            t_sum += thisone.t1;
            icounts += 1;
        }
    }
    if (icounts == 0) return;  // no a1 counts
    mgal_averages.push_back(mgal_sum/icounts);
    t_averages.push_back(t_sum/icounts);

    mgal_sum = 0;
    t_sum = 0;
    icounts = 0;
    for (val_time& thisone : gravtie->bcounts) {
        if (thisone.h1 > 0) {
            double mgalval = convert_counts_mgals(thisone,gravtie->lminfo.calib);
            thisone.m1 = mgalval;
            mgal_sum += mgalval;
            t_sum += thisone.t1;
            icounts += 1;
        }
    }
    if (icounts == 0) return;  // no b1 counts
    mgal_averages.push_back(mgal_sum/icounts);
    t_averages.push_back(t_sum/icounts);

    mgal_sum = 0;
    t_sum = 0;
    icounts = 0;
    for (val_time& thisone : gravtie->ccounts) {
        if (thisone.h1 > 0) {
            double mgalval = convert_counts_mgals(thisone,gravtie->lminfo.calib);
            thisone.m1 = mgalval;
            mgal_sum += mgalval;
            t_sum += thisone.t1;
            icounts += 1;
        }
    }
    if (icounts == 0) return;  // no a2 counts
    mgal_averages.push_back(mgal_sum/icounts);
    t_averages.push_back(t_sum/icounts);

    // use averaged times and mgals to do drift and tie calc
    double AA_timedelta = difftime(t_averages[2], t_averages[0]);
    double AB_timedelta = difftime(t_averages[1], t_averages[0]);
    double drift = (mgal_averages[2] - mgal_averages[0])/AA_timedelta;

    double dc_avg_mgals_B = mgal_averages[1] - AB_timedelta*drift;
    double gdiff = mgal_averages[0] - dc_avg_mgals_B;
    double land_tie_value = ref_g + gdiff;
    gravtie->lminfo.land_tie_value = land_tie_value;
    gravtie->drift = drift;
    gravtie->mgal_averages = mgal_averages;
    gravtie->t_averages = t_averages;

    char buffer[40]; // Adjust size?
    snprintf(buffer, sizeof(buffer), "Land tie value: %.2f", land_tie_value);
    gtk_label_set_text(GTK_LABEL(gravtie->lminfo.lt_label), buffer); 
}

