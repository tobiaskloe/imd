#include <stdio.h>
#include <math.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Core.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/SimpleMenu.h> 
#include <X11/Xaw/SmeBSB.h> 
#include <X11/Xaw/MenuButton.h> 
#include <X11/Xaw/SmeLine.h> 
#include <X11/Xaw/Dialog.h> 
#include "vogle.h"

#include "prototypes.h"
#include "globals.h"
#include "makros.h"

Widget	canvas,toplevel;
short int button;
int oldmousex,oldmousey,mousex,mousey;

struct drag_struct {
  GC   gc;
  GC   xor_gc;
  char *object;
} drag_client;

XtEventHandler press_event(Widget w,struct drag_struct *drag,XButtonEvent *ev) {
  button=ev->button;
  oldmousex=ev->x;
  oldmousey=ev->y;
}

XtEventHandler motion_event(Widget w,struct drag_struct *drag,XButtonEvent *ev) {
}

XtEventHandler release_event(Widget w,struct drag_struct *drag,XButtonEvent *ev) {
  int dx,dy,d;

  button=ev->button;
  mousex=ev->x;
  mousey=ev->y;

  dx=mousex-oldmousex;
  dy=mousey-oldmousey;
  d=sqrt(dx*dx+dy*dy);
  d=(dy>0)?d:-d;
  switch(button) {
  case 1: 
    translate(dx*.01,-dy*.01,.000001);
    draw_scene(scene_type);
    break;
  case 2: 
    rotate(dx*.1,'y');
    rotate(dy*.1,'x');
    draw_scene(scene_type);
    break;
  case 3: 
    scale(1+d*.01,1+d*.01,1+d*.01);
    draw_scene(scene_type);
    break;
  default:
    break;
  }
}

void LoadConfiguration(Widget w, XtPointer client, XtPointer call) {
  natoms=read_configuration("tmp.id");
  draw_scene(0);
}

void LoadDistribution(Widget w, XtPointer client, XtPointer call) {
  read_distribution("tmp.dist");
}

void SaveConfiguration(Widget w, XtPointer client, XtPointer call) {
  write_configuration("out.id");
}

void SaveDistribution(Widget w, XtPointer client, XtPointer call) {
  write_distribution("out.dist");
}

void SaveImage(Widget w, XtPointer client, XtPointer call) {
  system("import -window \"beavis2d\" out.gif");
}

void GetConfiguration(Widget w, XtPointer client, XtPointer call) {
  scene_type=0;
  if (connect_client(9)==0) {
    printf("No atoms!\n");
    exit(-1);
  }
  draw_scene(scene_type);
}

void GetDistribution(Widget w, XtPointer client, XtPointer call) {
  scene_type=1;
  if (connect_client(1)==0) {
    printf("No distribution!\n");
    exit(-1);
  }
  draw_scene(scene_type);
}

void DisplayAtoms(Widget w, XtPointer client, XtPointer call) {
  if (atom_mode) 
    atom_mode=0;
  else
    atom_mode=1;
  draw_scene(scene_type);
}

void DisplayBonds(Widget w, XtPointer client, XtPointer call) {
  if (bond_mode)
    bond_mode=0;
  else {
    bond_mode=1;
    if (stat_bond)
      read_unit_vectors();
  }
  draw_scene(scene_type);  
}

void SpecifyHost(Widget w, XtPointer client, XtPointer call) {
  printf("hallo %s\n",client);
  
}

void SpecifyPort(Widget w, XtPointer client, XtPointer call) {
  printf("hallo %s\n",client);
  
}

void Quit(Widget w, XtPointer client, XtPointer call) {
  exit(0);
}


#define SIZE	512


Display	*dpy;
Window	win;

XtCallbackProc quit() {
  vexit();
  exit(0);
}

resize() {
  Dimension       w, h;
  Arg             arg[2];

  XtSetArg(arg[0], XtNwidth, &w);
  XtSetArg(arg[1], XtNheight, &h);
  XtGetValues(canvas, arg, 2);

  fprintf(stderr, "resize() %d %d\n", w, h);
  vo_xt_win_size((int)w, (int)h);
  viewport(-1.0, 1.0, -1.0, 1.0);
}

repaint() {
  fprintf(stderr, "repaint() called\n");
  color(BLACK);
  clear();
}

XtActionsRec actions[] = {
  {"repaint", 	(XtActionProc)repaint},
  {"resize", 	(XtActionProc)resize}
};

String trans =
"<Expose>:	repaint()\n \
<Configure>:	resize()";


Display		*dpy;
Window		win;
GC		gc;

/*
 * simple program to display a polygon file
 */

window_main(int argc, char **argv) {
  int		w, h;
  Widget panel,
    qbut,
    bbut,
    fbut,
    dbut,
    hbut,
    xbut,
    ybut,
    zbut,
    sbut,
    pbut,
    mbut;


  Widget button1;
  Widget button2;
  Widget button3;
  Widget menu1;
  Widget menu2;
  Widget menu3;
  Widget line11;
  Widget line12;
  Widget line22;
  Widget entry;
  Widget LoadConf;
  Widget SaveConf;
  Widget LoadDist;
  Widget SaveDist;
  Widget SaveImg;
  Widget GetConf;
  Widget GetDist;
  Widget DispAts;
  Widget DispBds;
  Widget SpecHost;
  Widget SpecPort;
  Widget quit;

  int n;
  Arg wargs[5];
  XtTranslations trans_table;
  
  toplevel = XtInitialize(argv[0], "xtlcube", NULL, 0, &argc, argv);

  panel = XtCreateManagedWidget("panel",
				formWidgetClass,
				toplevel,
				NULL,
				0);
  
  /* create the pull down menu */
  button1 = XtCreateManagedWidget("File", menuButtonWidgetClass,
				  panel, NULL, 0);

  n = 0;
  XtSetArg(wargs[n], XtNmenuName, "menu1"); n++;
  XtSetValues(button1, wargs, n);

  button2 = XtCreateManagedWidget("Display", menuButtonWidgetClass,
				  panel, NULL, 0);

  n = 0;
  XtSetArg(wargs[n], XtNfromHoriz, button1); n++;
  XtSetArg(wargs[n], XtNmenuName, "menu2"); n++;
  XtSetValues(button2, wargs, n);

  button3 = XtCreateManagedWidget("Settings", menuButtonWidgetClass,
				  panel, NULL, 0);

  n = 0;
  XtSetArg(wargs[n], XtNfromHoriz, button2); n++;
  XtSetArg(wargs[n], XtNmenuName, "menu3"); n++;
  XtSetValues(button3, wargs, n);

  /* create the first pull down menu */

  menu1 = XtCreatePopupShell("menu1", simpleMenuWidgetClass,
			     button1, NULL, 0);
  LoadConf = XtCreateManagedWidget("Load Configuration", smeBSBObjectClass,
				menu1, NULL, 0);
  XtAddCallback(LoadConf, XtNcallback, LoadConfiguration, "Load Configuration");
  LoadDist = XtCreateManagedWidget("Load Distribution", smeBSBObjectClass,
				menu1, NULL, 0);
  XtAddCallback(LoadDist, XtNcallback, LoadDistribution, "Load Distribution");
  SaveConf = XtCreateManagedWidget("Save Configuration", smeBSBObjectClass,
				menu1, NULL, 0);
  XtAddCallback(SaveConf, XtNcallback, SaveConfiguration, "Save Configuration");
  SaveDist = XtCreateManagedWidget("Save Distribution", smeBSBObjectClass,
				menu1, NULL, 0);
  XtAddCallback(SaveDist, XtNcallback, SaveDistribution, "Save Distribution");
  SaveImg = XtCreateManagedWidget("Save Image", smeBSBObjectClass,
				menu1, NULL, 0);
  XtAddCallback(SaveImg, XtNcallback, SaveImage, "Save Image");
  line11 = XtCreateManagedWidget("line1", smeLineObjectClass,
				menu1, NULL, 0);
  GetConf = XtCreateManagedWidget("Get Configuration", smeBSBObjectClass,
				menu1, NULL, 0);
  XtAddCallback(GetConf, XtNcallback, GetConfiguration, "Get Configuration");
  GetDist = XtCreateManagedWidget("Get Distribution", smeBSBObjectClass,
				menu1, NULL, 0);
  XtAddCallback(GetDist, XtNcallback, GetDistribution, "Get Distribution");
  line12 = XtCreateManagedWidget("line12", smeLineObjectClass,
				menu1, NULL, 0);
  quit = XtCreateManagedWidget("quit", smeBSBObjectClass,
			       menu1, NULL, 0);

  XtAddCallback(quit, XtNcallback, Quit, NULL);

  /*
   *  create the second pull down menu
   */

  n = 0;
  menu2 = XtCreatePopupShell("menu2", simpleMenuWidgetClass,
			     button2, NULL, 0);

  DispAts = XtCreateManagedWidget("Display Atoms", smeBSBObjectClass,
				menu2, NULL, 0);
  XtAddCallback(DispAts, XtNcallback, DisplayAtoms, "DisplayAtoms");
  DispBds = XtCreateManagedWidget("Display Bonds", smeBSBObjectClass,
				menu2, NULL, 0);
  XtAddCallback(DispBds, XtNcallback, DisplayBonds, "DisplayBonds");
  line22 = XtCreateManagedWidget("line2", smeLineObjectClass,
				menu2, NULL, 0);

  quit = XtCreateManagedWidget("quit", smeBSBObjectClass,
			       menu2, NULL, 0);

  XtAddCallback(quit, XtNcallback, Quit, NULL);
  
  /*
   *  create the third pull down menu
   */

  menu3 = XtCreatePopupShell("menu3", simpleMenuWidgetClass,
			     button3, NULL, 0);

  SpecHost = XtCreateManagedWidget("Specify Host", smeBSBObjectClass,
				menu3, NULL, 0);
  XtAddCallback(SpecHost, XtNcallback, SpecifyHost, "Specify Host");
  SpecPort = XtCreateManagedWidget("Specify Port", smeBSBObjectClass,
				menu3, NULL, 0);
  XtAddCallback(SpecPort, XtNcallback, SpecifyPort, "Specify Port");
  line22 = XtCreateManagedWidget("line2", smeLineObjectClass,
				menu3, NULL, 0);

  quit = XtCreateManagedWidget("quit", smeBSBObjectClass,
			       menu3, NULL, 0);

  XtAddCallback(quit, XtNcallback, Quit, NULL);
  
  XtSetArg(wargs[0], XtNwidth, 512);
  XtSetArg(wargs[1], XtNheight, 512);
  canvas = XtCreateManagedWidget("canvas", 
				  simpleWidgetClass,
				  panel,
				  wargs,
				  2);

  XtAddActions(actions, XtNumber(actions));
  trans_table = XtParseTranslationTable(trans);
  XtAugmentTranslations(canvas, trans_table);

  XtAddEventHandler(panel, ButtonPressMask, FALSE,
		    press_event, &drag_client);
  XtAddEventHandler(panel, ButtonReleaseMask, FALSE,
		    release_event, &drag_client);
  XtAddEventHandler(panel, ButtonMotionMask, FALSE,
		    motion_event, &drag_client);

  XtRealizeWidget(toplevel);


  dpy = XtDisplay(canvas);
  win = XtWindow(canvas);

  vo_xt_window(dpy, win, 512, 512);
  vinit("X11");

  while(1) {
    XEvent	event;
		
    while(XtPending()) {
      XtNextEvent(&event);
      XtDispatchEvent(&event);
    }
    vo_xt_window(dpy, XtWindow(canvas), 512, 512);

  }
}



