#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <map>
#include <ctime>
#include "rw-general.h"
#include "rw-ties.h"
#include "tie_structs.h"

void on_dgs_filebrowse_clicked(GtkWidget *button, gpointer data) {
    ship_info* shinfo = static_cast<ship_info*>(data);
    if (shinfo->ship != "") {  // require that a ship be selected first
        //parent window for file chooser not strictly necessary though it is recommended
        GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Select File",
                                                               NULL, //GTK_WINDOW(user_data), 
                                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                                               "_Cancel",
                                                               GTK_RESPONSE_CANCEL,
                                                               "_Open",
                                                               GTK_RESPONSE_ACCEPT,
                                                               NULL);
        // enable multiple-file selection
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_chooser), TRUE);

        if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
            GSList *fileList = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(file_chooser));
            // Convert the GSList to a C++ vector
            std::vector<std::string> selectedFiles;
            for (GSList *files = fileList; files; files = g_slist_next(files)) {
                const gchar *filename = static_cast<const gchar *>(files->data);
                selectedFiles.push_back(filename);
            }
            // read the file into a pair of vectors
            std::pair<std::vector<float>, std::vector<time_t> > datadata = read_dat_dgs(selectedFiles,shinfo->ship);
            // push back into shinfo - can add to existing vector, will not ovrwrite
            shinfo->gravgrav.insert(shinfo->gravgrav.end(),datadata.first.begin(), datadata.first.end());
            shinfo->gravtime.insert(shinfo->gravtime.end(),datadata.second.begin(), datadata.second.end());
            g_slist_free_full(fileList, g_free);
        }
        gtk_widget_destroy(file_chooser);
//    } else { // no ship selected prior! remind:  // TODO (need to turn off highlighting with select)
//        GtkStyleContext *context;
//        context = gtk_widget_get_style_context(shinfo->cb1);
//        gtk_style_context_add_class(context, "highlighted");
        char dstring[30];
        sprintf(dstring,"  %i datapoints", (int) shinfo->gravgrav.size());
        gtk_label_set_text(GTK_LABEL(shinfo->dgs_label), dstring); 
    }
}

void on_lm_filebrowse_clicked(GtkWidget *button, gpointer data) {
    lm_info* lminfo = static_cast<lm_info*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Select File",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Open",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);

    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char* filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        // read the file into three vectors
        std::string sf = filename;
        calibration calib2 = read_lm_calib(sf);
        // push back into the external calib, overwriting
        lminfo->calib.brackets.clear();
        lminfo->calib.factors.clear();
        lminfo->calib.mgvals.clear();
        lminfo->calib.brackets = calib2.brackets;
        lminfo->calib.factors = calib2.factors;
        lminfo->calib.mgvals = calib2.mgvals;

        lminfo->cal_file_path = sf;
    }
    gtk_widget_destroy(file_chooser);
    //std::cout << lminfo->calib.brackets.size() << std::endl;
    char buffer[40]; // Adjust size?
    snprintf(buffer, sizeof(buffer), "%i calibration lines read", (int) lminfo->calib.brackets.size());
    gtk_label_set_text(GTK_LABEL(lminfo->cal_label), buffer); 

}



void on_savetie_clicked(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Save tie to file",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_SAVE,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Save",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_chooser), true);
    time_t rawtime;
    struct tm * cookedtime;
    char buffer[30];
    time (&rawtime);
    cookedtime = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d.toml", cookedtime);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), buffer);
    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char *savename;
        savename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        tie_to_toml(savename, gravtie);
    }

    gtk_widget_destroy(file_chooser);
}

void on_readtie_clicked(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Select File",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Open",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);

    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char* filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        toml_to_tie(filename, gravtie);
    }
    gtk_widget_destroy(file_chooser);

}

void on_write_report(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Save report to file",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_SAVE,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Save",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_chooser), true);
    time_t rawtime;
    struct tm * cookedtime;
    char buffer[30];
    time (&rawtime);
    cookedtime = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "tie_%Y%m%d.txt", cookedtime);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), buffer);
    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char *savename;
        savename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        write_report(savename, gravtie);
    }

    gtk_widget_destroy(file_chooser);
}

