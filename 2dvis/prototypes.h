#include <X11/Intrinsic.h>
void read_parameters(int argc,char **argv);
void usage(void);
void make_picture(void);
void draw_scene(int scene_type);
void compute_bonds(void);
void init_graph(void);
void display_help(void);
void read_unit_vectors(void);
void write_to_file(void);
void getparamfile(char *paramfname);
void error(char *mesg);
void LoadConfiguration(Widget w, XtPointer client, XtPointer call);
void LoadDistribution(Widget w, XtPointer client, XtPointer call);
void SaveConfiguration(Widget w, XtPointer client, XtPointer call);
void SaveDistribution(Widget w, XtPointer client, XtPointer call);
void SaveImage(Widget w, XtPointer client, XtPointer call);
void GetConfiguration(Widget w, XtPointer client, XtPointer call);
void GetDistribution(Widget w, XtPointer client, XtPointer call);
void DisplayAtoms(Widget w, XtPointer client, XtPointer call);
void DisplayBonds(Widget w, XtPointer client, XtPointer call);
void SpecifyHost(Widget w, XtPointer client, XtPointer call);
void SpecifyPort(Widget w, XtPointer client, XtPointer call);
void Quit(Widget w, XtPointer client, XtPointer call);
void draw_scene(int scene_type);
void draw_bonds(void);
void draw_text(void);

int read_configuration(char *fname);
int write_configuration(char *fname);
int read_distribution(char *fname);
int write_distribution(char *fname);

