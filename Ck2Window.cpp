#include "Ck2Window.hh"
#include <QPainter> 
#include "Parser.hh"
#include <cstdio> 
#include <QtGui>
#include <QDesktopWidget>
#include <QRect>
#include <qpushbutton.h>
#include <iostream> 
#include <string>
#include "Logger.hh" 
#include <set>
#include <algorithm>
#include "StructUtils.hh" 
#include "StringManips.hh"
#include <direct.h>

using namespace std; 

char stringbuffer[10000];
CleanerWindow* parentWindow;

int main (int argc, char** argv) {
  QApplication industryApp(argc, argv);
  QDesktopWidget* desk = QApplication::desktop();
  QRect scr = desk->availableGeometry();
  parentWindow = new CleanerWindow();
  parentWindow->show();
  srand(42); 
  
  parentWindow->resize(3*scr.width()/5, scr.height()/2);
  parentWindow->move(scr.width()/5, scr.height()/4);
  parentWindow->setWindowTitle(QApplication::translate("toplevel", "CK2 parser utility"));
 
  QMenuBar* menuBar = parentWindow->menuBar();
  QMenu* fileMenu = menuBar->addMenu("File");
  QAction* newGame = fileMenu->addAction("Load file");
  QAction* quit    = fileMenu->addAction("Quit");

  QObject::connect(quit, SIGNAL(triggered()), parentWindow, SLOT(close())); 
  QObject::connect(newGame, SIGNAL(triggered()), parentWindow, SLOT(loadFile())); 

  QMenu* actionMenu = menuBar->addMenu("Actions");
  QAction* clean = actionMenu->addAction("Clean");
  QObject::connect(clean, SIGNAL(triggered()), parentWindow, SLOT(cleanFile())); 
  QAction* stats = actionMenu->addAction("Stats");
  QObject::connect(stats, SIGNAL(triggered()), parentWindow, SLOT(getStats()));
  QAction* convert3 = actionMenu->addAction("Convert to EU3");
  QObject::connect(convert3, SIGNAL(triggered()), parentWindow, SLOT(convertEU3()));
  QAction* convert4 = actionMenu->addAction("Convert to EU4");
  QObject::connect(convert4, SIGNAL(triggered()), parentWindow, SLOT(convertEU4())); 
  
  QAction* cmap = actionMenu->addAction("Colour map");
  QObject::connect(cmap, SIGNAL(triggered()), parentWindow, SLOT(colourMap()));
  
  parentWindow->textWindow = new QPlainTextEdit(parentWindow);
  parentWindow->textWindow->setFixedSize(3*scr.width()/5 - 10, scr.height()/2-40);
  parentWindow->textWindow->move(5, 30);
  parentWindow->textWindow->show(); 

  Logger::createStream(Logger::Debug);
  Logger::createStream(Logger::Trace);
  Logger::createStream(Logger::Game);
  Logger::createStream(Logger::Warning);
  Logger::createStream(Logger::Error);

  QObject::connect(&(Logger::logStream(Logger::Debug)),   SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Trace)),   SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Game)),    SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Warning)), SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Error)),   SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));

  setOutputStream(&(Logger::logStream(Logger::Game))); 
  
  for (int i = DebugLeaders; i < NumDebugs; ++i) {
    Logger::createStream(i);
    QObject::connect(&(Logger::logStream(i)),   SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
    Logger::logStream(i).setActive(false); 
  } 

  parentWindow->show();
  if (argc > 1) parentWindow->loadFile(argv[1], argc > 2 ? atoi(argv[2]) : 1);  
  return industryApp.exec();
}


CleanerWindow::CleanerWindow (QWidget* parent) 
  : QMainWindow(parent)
  , worker(0)
{}

CleanerWindow::~CleanerWindow () {}

void CleanerWindow::message (QString m) {
  textWindow->appendPlainText(m);
  if (debuglog.is_open()) debuglog << m.toAscii().data() << std::endl; 
}

void CleanerWindow::loadFile () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString(""), QString("*.ck2"));
  string fn(filename.toAscii().data());
  if (fn == "") return;
  loadFile(fn);   
}

void CleanerWindow::loadFile (string fname, int autoTask) {
  if (worker) delete worker;
  worker = new WorkerThread(fname, autoTask);
  worker->start();
}

void CleanerWindow::cleanFile () {
  Logger::logStream(Logger::Game) << "Starting clean.\n";
  worker->setTask(WorkerThread::CleanFile); 
  worker->start(); 
}

void CleanerWindow::getStats () {
  Logger::logStream(Logger::Game) << "Starting statistics.\n";
  worker->setTask(WorkerThread::Statistics); 
  worker->start(); 
}

void CleanerWindow::convertEU3 () {
  Logger::logStream(Logger::Game) << "Convert to EU3.\n";
  worker->setTask(WorkerThread::ConvertEu3); 
  worker->start(); 
}

void CleanerWindow::convertEU4 () {
  Logger::logStream(Logger::Game) << "Convert to EU4.\n";
  worker->setTask(WorkerThread::ConvertEu4); 
  worker->start(); 
}


void CleanerWindow::colourMap() {
  if (worker) delete worker;
  worker = new WorkerThread("blah"); 
  Logger::logStream(Logger::Game) << "Map colours.\n"; 
  worker->setTask(WorkerThread::ColourMap); 
  worker->start(); 
}

void CleanerWindow::closeDebugLog () {
  if (debuglog.is_open()) debuglog.close();
}

void CleanerWindow::writeDebugLog (string fname) {
  if (fname == "") return;
  if (debuglog.is_open()) debuglog.close();
  debuglog.open(fname.c_str()); 
  /*
  QFile outfile;
  outfile.setFileName(QString(fname.c_str()));
  outfile.open(QIODevice::Append | QIODevice::Text);
  QTextStream out(&outfile);

  QTextDocument* doc = textWindow->document();
  for (QTextBlock b = doc->begin(); b != doc->end(); b = b.next()) {
    out << b.text() << endl;
  }
  outfile.close();
  */
}

WorkerThread::WorkerThread (string fn, int atask)
  : targetVersion(".\\Vanilla\\")
  , sourceVersion(".\\ckvanilla\\")
  , fname(fn)
  , euxGame(0)
  , ck2Game(0)
  , task(LoadFile)
  , configObject(0)
  , autoTask(atask)
{
  configure();
}  

WorkerThread::~WorkerThread () {
  if (euxGame) delete euxGame;
  if (ck2Game) delete ck2Game; 
  euxGame = 0;
  ck2Game = 0; 
}

void WorkerThread::run () {
  switch (task) {
  case LoadFile: loadFile(fname); break;
  case CleanFile: cleanFile(); break;
  case Statistics: getStatistics(); break;
  case ConvertEu3: convertEU3(); break;
  case ConvertEu4: convertEU4(); break;    
  case ColourMap: colourMap(); break; 
  case NumTasks: 
  default: break; 
  }
}

string convertMonth (string month) {
  if (month == "1")       return "january";
  else if (month == "2")  return "february";
  else if (month == "3")  return "march";
  else if (month == "4")  return "april";
  else if (month == "5")  return "may";
  else if (month == "6")  return "june";
  else if (month == "7")  return "july";
  else if (month == "8")  return "august";
  else if (month == "9")  return "september";
  else if (month == "10") return "october";
  else if (month == "11") return "november";
  else if (month == "12") return "december";
  else return "january"; 
}

string convertMonth (int month) {
  if (month == 1)       return "january";
  else if (month == 2)  return "february";
  else if (month == 3)  return "march";
  else if (month == 4)  return "april";
  else if (month == 5)  return "may";
  else if (month == 6)  return "june";
  else if (month == 7)  return "july";
  else if (month == 8)  return "august";
  else if (month == 9)  return "september";
  else if (month == 10) return "october";
  else if (month == 11) return "november";
  else if (month == 12) return "december";
  else return "january"; 
}

class ScatterPlot {
public:
  ScatterPlot (const char* xaxis, const char* yaxis);
  void addFlag (QPixmap* flag, double xc, double yc); 
  void paint (const char* filename);
private:
  QPainter* painter;
  QImage* image;
  vector<pair<double, double> > positions;
  vector<QPixmap*> flags;
  pair<double, double> norm; 
};

ScatterPlot::ScatterPlot (const char* xaxis, const char* yaxis) {
  image = new QImage(500, 500, QImage::Format_RGB32);
  painter = new QPainter(image);
  QPen pen(Qt::white);
  QBrush brush;
  brush.setStyle(Qt::SolidPattern);
  brush.setColor(Qt::white); 
  painter->setPen(pen);
  painter->setBrush(brush); 
  painter->drawRect(0, 0, 500, 500);

  pen.setColor(Qt::black);
  painter->setPen(pen);
  painter->drawLine(20, 0, 20,  500);
  painter->drawLine(0, 480, 500, 480);

  painter->drawText(235, 491, xaxis);
  painter->rotate(-90);
  painter->drawText(-200, 8, yaxis); 
  painter->rotate(90);   
}

void ScatterPlot::paint (const char* filename) {
  for (unsigned int i = 0; i < flags.size(); ++i) {
    int xpos = (int) floor(20 + 470*positions[i].first/norm.first + 0.5);
    int ypos = (int) floor(480 - 470*positions[i].second/norm.second + 0.5);
    painter->drawPixmap(QRect(xpos - 10, ypos - 10, 20, 20), *(flags[i]), QRect(0, 0, 128, 128));
  }
  painter->drawText(1, 491, "  0");
  painter->drawText(1, 235, " 50");
  painter->drawText(1, 20,  "100");
  sprintf(stringbuffer, "%i", (int) floor(norm.first + 0.5));
  painter->drawText(470, 490, stringbuffer);
  image->save(filename); 
}

void ScatterPlot::addFlag (QPixmap* flag, double xc, double yc) {
  positions.push_back(pair<double, double>(xc, yc));
  flags.push_back(flag);
  norm.first = max(norm.first, xc);
  norm.second = max(norm.second, yc); 
}

class TriangleImage {
public:
  TriangleImage (const char* dl, const char* dr, const char* up); 
  void addFlag (QPixmap* flag, double dl, double dr, double up, string name);
  void paint (const char* filename);
  void reconcile (); 
private:
  QPainter* painter;
  QImage* image;
  vector<pair<double, double> > originalPositions;
  vector<pair<double, double> > finalPositions;
  vector<QPixmap*> flags;
  vector<string> names; 
  vector<double> downlefts;
  vector<double> downright;
  vector<double> upamounts; 
  
  void push ();
  double smallestDistance ();  
};

void TriangleImage::addFlag (QPixmap* flag, double dl, double dr, double up, string name) {
  double xpos = 250;
  double ypos = 315;
  double scale = 1.0 / (dl + dr + up);
  xpos -= floor(scale * dl * 240 + 0.5);
  xpos += floor(scale * dr * 240 + 0.5);
  ypos -= floor(scale * up * 277 + 0.5);
  ypos += floor(scale * dl * 138 + 0.5); 
  ypos += floor(scale * dr * 138 + 0.5);

  originalPositions.push_back(pair<double, double>(xpos, ypos));
  finalPositions.push_back(pair<double, double>(xpos, ypos));
  flags.push_back(flag);
  names.push_back(name);
  downlefts.push_back(dl);
  downright.push_back(dr);
  upamounts.push_back(up); 
}

double TriangleImage::smallestDistance () {
  double smallest = 1e20; 
  for (vector<pair<double, double> >::iterator i = finalPositions.begin(); i != finalPositions.end(); ++i) {
    for (vector<pair<double, double> >::iterator j = finalPositions.begin(); j != finalPositions.end(); ++j) {
      if (i == j) continue;
      double dist = pow((*i).first - (*j).first, 2);
      dist       += pow((*i).second - (*j).second, 2);
      dist        = sqrt(dist);
      if (dist < smallest) smallest = dist;
    }
  }
  return smallest; 
}

void TriangleImage::push () {
  for (unsigned int i = 0; i < finalPositions.size(); ++i) {
    for (unsigned int j = 0; j < finalPositions.size(); ++j) {    
      if (i == j) {
	double xdist = finalPositions[i].first - originalPositions[j].first;
	double ydist = finalPositions[i].second - originalPositions[j].second;
	finalPositions[i].first  -= xdist * 0.01;
	finalPositions[i].second -= ydist * 0.01;	
      }
      else {
	double xdist = finalPositions[i].first - finalPositions[j].first;
	double ydist = finalPositions[i].second - finalPositions[j].second;
	double dist  = pow(xdist, 2);
	dist        += pow(ydist, 2);
	dist         = 0.00001 + sqrt(dist);
	if (dist > 35) continue; 
	finalPositions[i].first  += xdist * (1 / dist); 
	finalPositions[i].second += ydist * (1 / dist);
	finalPositions[j].first  -= xdist * (1 / dist); 
	finalPositions[j].second -= ydist * (1 / dist); 	
      }
    }
  }  
}

void TriangleImage::reconcile () {
  for (int i = 0; i < 50000; ++i) {
    double curr = smallestDistance();
    if (curr > 23) break;
    push();
    if (0 == i % 1000) Logger::logStream(Logger::Game) << "Reconcile " << i << " " << curr << ".\n";
  }
}

void TriangleImage::paint (const char* filename) {
  QPen wpen(Qt::white);
  QPen bpen(Qt::black);  

  for (unsigned int i = 0; i < flags.size(); ++i) {
    int xpos = (int) floor(finalPositions[i].first + 0.5);
    int ypos = (int) floor(finalPositions[i].second + 0.5);
    painter->setPen(bpen);        
    painter->drawLine(xpos, ypos, originalPositions[i].first, originalPositions[i].second);     
    painter->drawPixmap(QRect(xpos - 10, ypos - 10, 20, 20), *(flags[i]), QRect(0, 0, 128, 128));

    painter->drawPixmap(QRect(5 + ((i%2)*295), 5+22*(i/2), 20, 20), *(flags[i]), QRect(0, 0, 128, 128));
    painter->setPen(wpen);      
    sprintf(stringbuffer, "%s (%.1f, %.1f, %.1f)", names[i].c_str(), downlefts[i], downright[i], upamounts[i]);
    painter->drawText(28 + ((i%2)*295), 16+22*(i/2), stringbuffer);      
  }

  painter->setPen(bpen);    
  for (unsigned int i = 0; i < flags.size(); ++i) {
    int xpos = (int) floor(originalPositions[i].first + 0.5);
    int ypos = (int) floor(originalPositions[i].second + 0.5);
    painter->drawEllipse(xpos, ypos, 4, 4); 
  }  
  image->save(filename); 
}

TriangleImage::TriangleImage (const char* dl, const char* dr, const char* up) {
  image = new QImage(500, 500, QImage::Format_RGB32);
  painter = new QPainter(image);
  QPen pen(Qt::white);
  QBrush brush;
  brush.setStyle(Qt::SolidPattern);
  brush.setColor(Qt::white); 
  painter->setPen(pen);
  painter->setBrush(brush); 
  QPolygon triangle(3); 
  triangle.putPoints(0, 3, 10, 454, 490, 454, 250, 46);
  painter->drawPolygon(triangle);
  painter->drawText(5, 484, dl);
  painter->drawText(470, 484, dr);
  painter->drawText(235, 16, up);
  pen.setColor(Qt::black);
  painter->setPen(pen);
  painter->drawLine(250, 315, 250,  46);
  painter->drawLine(250, 315,  10, 454);
  painter->drawLine(250, 315, 490, 454);
}

int calcLevel (Object* btype, Object* buildings) {
  int level = btype->safeGetInt("level", -1);
  if (-1 != level) return level; 

  level = 0; 
  Object* upgraded = buildings->safeGetObject(btype->safeGetString("upgrades_from", "BLAH"));
  if (upgraded) level += 1 + calcLevel(upgraded, buildings);
  
  Object* prereqs = btype->safeGetObject("prerequisites");
  if (prereqs) {
    for (int token = 0; token < prereqs->numTokens(); ++token) {
      upgraded = buildings->safeGetObject(prereqs->getToken(token));
      if (!upgraded) continue;
      level += 1 + calcLevel(upgraded, buildings);
    }
  }
  
  btype->resetLeaf("level", level);
  return level; 
}

void WorkerThread::getStatistics () {
  if (!ck2Game) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return; 
  }

  initialiseCharacters(); 
  initialiseRelationMaps();
  loadFiles();
  createVassalsMap(); 

  objvec players;
  for (objiter c = begin(Chars); c != final(Chars); ++c) {
    if ((*c)->safeGetString("player", "no") != "yes") continue;
    players.push_back(*c); 
  }

  objvec leaves = ck2Game->getLeaves();
  for (objiter prov = leaves.begin(); prov != leaves.end(); ++prov) {
    int maxSettlements = (*prov)->safeGetInt("max_settlements", -1); 
    if (-1 == maxSettlements) continue;

    Object* title = getTitle(remQuotes((*prov)->safeGetString("title")));
    Object* player = 0;
    while (title) {
      Object* holder = getChar(title->safeGetString("holder"));
      if (find(players.begin(), players.end(), holder) != players.end()) {
	player = holder;
	break;
      }
      title = liegeMap[title]; 
    }

    if (!player) continue; 
    player->resetLeaf("provinces", 1 + player->safeGetInt("provinces"));
    player->resetLeaf("max_holdings", maxSettlements + player->safeGetInt("max_holdings")); 
    
    objvec holdings = (*prov)->getLeaves(); 
    for (objiter holding = holdings.begin(); holding != holdings.end(); ++holding) {
      string htype = (*holding)->safeGetString("type");
      if (htype == "castle") player->resetLeaf("castle", 1 + player->safeGetInt("castle"));
      else if (htype == "city") player->resetLeaf("city", 1 + player->safeGetInt("city"));
      else if (htype == "temple") player->resetLeaf("temple", 1 + player->safeGetInt("temple"));
      else continue;

      Object* buildings = ckBuildings->safeGetObject(htype);
      if (!buildings) continue;
      objvec btypes = buildings->getLeaves();
      for (objiter btype = btypes.begin(); btype != btypes.end(); ++btype) {
	if ((*holding)->safeGetString((*btype)->getKey(), "no") != "yes") continue;
	player->resetLeaf("totalBuildings", 1 + player->safeGetInt("totalBuildings"));
	int level = calcLevel(*btype, buildings);
	player->resetLeaf("totalLevels", level + player->safeGetInt("totalLevels"));
      }
    }
  }

  for (objiter player = players.begin(); player != players.end(); ++player) {
    TitleTier highest = Barony;
    Object* title = 0;
    for (objiter t = beginRel((*player), Title); t != finalRel((*player), Title); ++t) {
      if ((!title) || (titleTier(*t) > highest)) {
	highest = titleTier(*t);
	title = (*t); 
      }
    }

    (*player)->resetLeaf("title", title->getKey()); 
  }

  
  TriangleImage holdings("Castle", "City", "Temple");
  ScatterPlot settled("Total baronies", "Settled percentage");
  ScatterPlot techlevel("Total buildings", "Average level"); 
  for (objiter player = players.begin(); player != players.end(); ++player) {
    Logger::logStream(Logger::Game) << "Flagging " << (*player)->safeGetString("title") << "\n";
    string flagname(".\\flags\\");
    flagname += (*player)->safeGetString("title");
    flagname += ".bmp";
    DWORD attribs = GetFileAttributesA(flagname.c_str());
    if (attribs == INVALID_FILE_ATTRIBUTES) flagname = "./flags/k_norway.bmp";
    QPixmap* currflag = new QPixmap(flagname.c_str()); 
    
    double cities = (*player)->safeGetFloat("city");
    double castle = (*player)->safeGetFloat("castle");
    double temple = (*player)->safeGetFloat("temple");
    double barony = (*player)->safeGetFloat("max_holdings");
    double levels = (*player)->safeGetFloat("totalLevels");
    double builds = (*player)->safeGetFloat("totalBuildings");
    //double provinces = (*player)->safeGetFloat("provinces");

    settled.addFlag(currflag, barony, (cities + castle + temple)/barony); 
    techlevel.addFlag(currflag, builds, levels / builds); 
    holdings.addFlag(currflag, castle, cities, temple, (*player)->safeGetString("title"));
  }

  holdings.reconcile();
  holdings.paint("holdings.png");
  
  settled.paint("development.png");
  techlevel.paint("buildings.png"); 
  
  Logger::logStream(Logger::Game) << "Done with statistics.\n";
}

void WorkerThread::cleanFile () {
  Logger::logStream(Logger::Game) << "Cleaning file " << (int) ck2Game << ".\n"; 
  if (!ck2Game) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return; 
  }

  Object* charList = ck2Game->safeGetObject("character");
  assert(charList); 
  int numRemoved = 0;
  objiter dummy = begin(Chars);
  for (objiter c = beginRel(*dummy, Child); c != finalRel(*dummy, Child); ++c) {
    numRemoved++;
  }
  numRemoved = 0; 

  map<string, int> safeList;
  objvec hists = ck2Game->getValue("character_history");
  Logger::logStream(Logger::Game) << "History size: " << hists.size() << "\n"; 
  for (objiter h = hists.begin(); h != hists.end(); ++h) {
    objvec leaves = (*h)->getLeaves();
    for (objiter l = leaves.begin(); l != leaves.end(); ++l) {
      safeList[(*l)->safeGetString("identity")]++; 
    }
  }

  for (objiter c = begin(Chars); c != final(Chars); ++c) {
    Object* unborn = (*c)->safeGetObject("unborn");
    if (!unborn) continue;
    objvec unlist = unborn->getLeaves();
    if (0 == unlist.size()) continue;
    Object* father = unlist[0]->safeGetObject("father");
    safeList[father->safeGetString("id")]++; 
  }

  
  for (objiter c = begin(Chars); c != final(Chars); ++c) {
    if (0 < children[*c].size()) continue;
    if ((*c)->safeGetString("death_date") == "") continue;
    if (0 < safeList[(*c)->getKey()]) continue;    
    if (0 == numRemoved % 1) Logger::logStream(Logger::Game) << "Removing " << (*c)->getKey() << "\n"; 
    
    charList->removeObject(*c); 
    
    numRemoved++;
  }

  ofstream writer;
  writer.open(".\\Output\\cleaned.ck2");
  Parser::topLevel = ck2Game;
  writer << (*ck2Game);
  writer.close();
  
  Logger::logStream(Logger::Game) << "Done cleaning. Removed " << numRemoved << " characters.\n";
}

void WorkerThread::loadFile (string fname) {
  ck2Game = loadTextFile(fname);
  Logger::logStream(Logger::Game) << "Done processing " << (int) ck2Game << " " << autoTask << ".\n";
  switch (autoTask) {
  case 1:
    task = ConvertEu3;
    convertEU3(); 
    break;
  case 2:
    task = Statistics;
    getStatistics();
    break;
  case 5:
    task = ColourMap;
    colourMap(); 
    break;
  case -1: 
  default:
    break;
  }
}

void WorkerThread::colourMap () {}


void WorkerThread::configure () {
  configObject = processFile("config.txt");
  targetVersion = configObject->safeGetString("eu3dir", ".\\DW\\");
  sourceVersion = configObject->safeGetString("ck2dir", ".\\Vanilla\\");
  Logger::logStream(Logger::Debug).setActive(false);

  Object* debug = configObject->safeGetObject("debug");
  if (debug) {
    if (debug->safeGetString("generic", "no") == "yes") Logger::logStream(Logger::Debug).setActive(true);
    bool activateAll = (debug->safeGetString("all", "no") == "yes");
    for (int i = DebugLeaders; i < NumDebugs; ++i) {
      sprintf(stringbuffer, "%i", i);
      if ((activateAll) || (debug->safeGetString(stringbuffer, "no") == "yes")) Logger::logStream(i).setActive(true);
    }
  }
}

Object* WorkerThread::loadTextFile (string fname) {
  Logger::logStream(Logger::Game) << "Parsing file " << fname << "\n";
  ifstream reader;
  reader.open(fname.c_str());
  if ((reader.eof()) || (reader.fail())) {
    Logger::logStream(Logger::Error) << "Could not open file, returning null object.\n";
    return 0; 
  }
  
  Object* ret = processFile(fname);
  Logger::logStream(Logger::Game) << " ... done.\n";
  return ret; 
}


objiter WorkerThread::begin (IterType it) {
  if (!ck2Game) {
    Logger::logStream(Logger::Error) << "Attempt to iterate over " << it << " with no game loaded. Probably indicates an error.\n";
  }
  else {
    static bool initialised = false;
    if (!initialised) {
      initialiseCharacters();
      initialised = true; 
    }
  }

  
  switch (it) {
  default:
  case Chars:  return characters.begin();
  case Titles: return titles.begin();
  }

  return characters.begin(); 
}

objiter WorkerThread::final (IterType it) {
  switch (it) {
  default:
  case Chars:  return characters.end();
  case Titles: return titles.end();
  }
  
  return characters.end(); 
}


objiter WorkerThread::beginRel (Object* c, RelationType r) {
  if (!ck2Game) {
    Logger::logStream(Logger::Error) << "Attempt to extract relations with no game loaded. Probably indicates an error.\n";
    return characters.begin(); 
  }

  static bool initialised = false;
  if (!initialised) {
    initialised = true;
    initialiseRelationMaps();     
  }

  switch (r) {
  default: 
  case Father:   return fathers[c].begin();
  case Mother:   return mothers[c].begin();
  case Child:    return children[c].begin();
  case Son:      return sons[c].begin();
  case Daughter: return daughters[c].begin();
  case Title:    return charTitles[c].begin();
  }
  
}

objiter WorkerThread::finalRel (Object* c, RelationType r) {
  switch (r) {
  default: 
  case Father:   return fathers[c].end();
  case Mother:   return mothers[c].end();
  case Child:    return children[c].end();
  case Son:      return sons[c].end();
  case Daughter: return daughters[c].end();
  case Title:    return charTitles[c].end();    
  }
}

WorkerThread::TitleTier WorkerThread::titleTier (Object* title) {
  switch (title->getKey()[0]) {
  case 'e': return Empire;
  case 'k': return Kingdom;
  case 'd': return Duchy;
  case 'c': return County;
  case 'b': return Barony;
  default:
    return Other; 
  }
}

string nameAndNumber (Object* eu3prov) {
  return eu3prov->getKey() + " (" + remQuotes(eu3prov->safeGetString("name", "\"could not find name\"")) + ")";
}

/********************************  End helpers  **********************/

/******************************** Begin initialisers **********************/

void WorkerThread::assignCKprovinces () {
  // Assign ownership of CK provinces to CK titles 
  for (objiter ckp = ck2provs.begin(); ckp != ck2provs.end(); ++ckp) {
    string tag = remQuotes((*ckp)->safeGetString("title"));
    if (tag == "") {
      Logger::logStream(Logger::Warning) << "Warning: Could not find title of province "
					 << (*ckp)->getKey()
					 << ".\n";
      continue;
    }

    Object* title = ck2Game->safeGetObject(tag);
    if (!title) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find title " << tag
					 << ", wanted for ownership of province "
					 << (*ckp)->getKey() << ".\n";
      continue;
    }

    title->setLeaf("ckProvince", (*ckp)->getKey());
    string holderId = title->safeGetString("holder");
    Object* holder = getChar(holderId);
    if (!holder) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find character " << holderId
					 << ", wanted for ownership of province "
					 << (*ckp)->getKey() << ".\n";
      continue;
    }
    (*ckp)->setLeaf("owner", holderId); 
  }
}

void WorkerThread::createCountryMap () {
  set<Object*> unusedEuCountries; 
  set<Object*> unConvertedCharacters; 

  Object* skips = configObject->safeGetObject("skip"); 
  
  // Backup to find titles that are not in the link-list file. 
  for (objiter title = begin(Titles); title != final(Titles); ++title) {
    Object* holder = titleToCharMap[*title];
    if ((skips) && (skips->safeGetString((*title)->getKey(), "no") == "yes")) continue; // Mercenaries     
    if (!holder) continue; // This is normal, indicates eg uncreated empire.
    if (Barony == titleTier(*title)) continue;
    unConvertedCharacters.insert(holder); 
  }
  
  for (objiter link = countryLinks.begin(); link != countryLinks.end(); ++link) {
    string cktag = (*link)->safeGetString("CK2");
    Object* title = getTitle(cktag);
    if (!title) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find title " << cktag << " for conversion.\n";
      continue;
    }

    Object* holder = titleToCharMap[title];
    if (!holder) continue; // This is normal, indicates eg uncreated empire. 
    Object* demesne = holder->safeGetObject("demesne");
    if (demesne) {
      string primary = remQuotes(demesne->safeGetString("primary", "BAH"));
      if ((primary != "BAH") && (primary != cktag)) continue;
    }
    // Sometimes primary title is not listed. Check that this is a highest-level title for the character.
    TitleTier thisTier = titleTier(title);
    TitleTier bestTier = thisTier; 
    for (objiter t = beginRel(holder, Title); t != finalRel(holder, Title); ++t) {
      TitleTier currTier = titleTier(*t);
      if (currTier <= bestTier) continue;
      bestTier = currTier;
    }
    if (bestTier > thisTier) continue; 
    
    string eutag = (*link)->safeGetString("EUX");
    Object* country = euxGame->safeGetObject(eutag);
    if (!country) {
      string playertag = remQuotes(euxGame->safeGetString("player"));
      Object* existingCountry = euxGame->safeGetObject(playertag);
      if (!existingCountry) existingCountry = (characterToEuCountryMap.empty() ? 0 : (*characterToEuCountryMap.begin()).second);
      if (!existingCountry) existingCountry = unusedEuCountries.empty() ? 0 : *(unusedEuCountries.begin());
      if (!existingCountry) {
	Logger::logStream(Logger::Warning) << "Warning: Could not find country " << eutag
					   << ", failed to make a clone. (Suggest inserting a line 'player=\"XXX\"' at start of input.eug.) Ignoring "
					   << cktag << " conversion.\n";
	continue; 
      }
       
      country = new Object(existingCountry);
      country->setKey(eutag);
      euxGame->setValue(country, existingCountry);
      Object* history = country->getNeededObject("history");
      history->clear();
      country->unsetValue("army");
      country->unsetValue("navy");
      country->unsetValue("monarch");
      country->unsetValue("technology"); 
      
      Logger::logStream(Logger::Game) << "Created clone of "
				      << existingCountry->getKey()
				      << " for tag " << eutag
				      << ".\n"; 
	
    }

    if (euCountryToCharacterMap[country]) {
      if (!characterToEuCountryMap[holder]) unConvertedCharacters.insert(holder); 
      continue; // Already used this tag.
    }
    if (characterToEuCountryMap[holder]) {
      unusedEuCountries.insert(country);
      continue;  // Already converted character.
    }

    unusedEuCountries.erase(country);
    unConvertedCharacters.erase(holder);
    characterToEuCountryMap[holder] = country;
    euCountryToCharacterMap[country] = holder;
    euCountryToCkCountryMap[country] = title; 

    Logger::logStream(DebugCountries) << "Assigned " << cktag << " to " << eutag << ".\n";
  }

  while (unConvertedCharacters.size()) {
    int numTitles = 0;
    Object* best = (*unConvertedCharacters.begin());
    for (set<Object*>::iterator i = unConvertedCharacters.begin(); i != unConvertedCharacters.end(); ++i) {
      int points = 0;
      for (objiter title = beginRel((*i), Title); title != finalRel((*i), Title); ++title) {
	if (find(sovereigns.begin(), sovereigns.end(), *title) != sovereigns.end()) points += 150; 	
	switch (titleTier(*title)) {
	case Barony:  points += 5;   break; 
	case County:  points += 10;  break;
	case Duchy:   points += 30;  break;
	case Kingdom: points += 50;  break;
	case Empire:  points += 100; break;
	  
	case Other:
	default:
	  break; 
	}
      }
      
      if (points < numTitles) continue;
      numTitles = points;
      best = (*i); 
    }
    assert(best);
    Object* primaryTitle = charTitles[best][0];
    assert(primaryTitle);    
    unConvertedCharacters.erase(best); 
    if (0 < unusedEuCountries.size()) {
      Object* euc = *(unusedEuCountries.begin());
      unusedEuCountries.erase(euc);
      euCountryToCharacterMap[euc]  = best;
      characterToEuCountryMap[best] = euc;
      euCountryToCkCountryMap[euc]  = primaryTitle; 
      Logger::logStream(DebugCountries) << "Backup-assigned " << primaryTitle->getKey() << " to " << euc->getKey() << ".\n"; 
      continue;
    }

    // Nothing to be done about it, absorb him into the liege. 
    Object* liegeTitle = liegeMap[primaryTitle];
    while (liegeTitle) {
      Object* liege = titleToCharMap[liegeTitle];
      if (characterToEuCountryMap[liege]) {
	characterToEuCountryMap[best] = characterToEuCountryMap[liege];
	Logger::logStream(DebugCountries) << "Absorbed " << primaryTitle->getKey()
					  << " into overlord "
					  << liegeTitle->getKey()
					  << " and therefore assigned to "
					  << characterToEuCountryMap[liege]->getKey()
					  << ".\n"; 
	break;
      }
      liegeTitle = liegeMap[liegeTitle]; 
    }
    if (characterToEuCountryMap[best]) continue; 

    // Check for de-jure liege. 
    string dejureTag = remQuotes(primaryTitle->safeGetString("de_jure_liege"));
    liegeTitle = getTitle(dejureTag);
    while (liegeTitle) {
      Object* liege = titleToCharMap[liegeTitle];
      if (characterToEuCountryMap[liege]) {
	characterToEuCountryMap[best] = characterToEuCountryMap[liege];
	Logger::logStream(DebugCountries) << "Absorbed " << primaryTitle->getKey()
					  << " into de-jure liege "
					  << liegeTitle->getKey()
					  << " and therefore assigned to "
					  << characterToEuCountryMap[liege]->getKey()
					  << ".\n"; 
	break;
      }
      liegeTitle = liegeMap[liegeTitle];
      if (!liegeTitle) {
	dejureTag = remQuotes(liegeTitle->safeGetString("de_jure_liege"));
	liegeTitle = getTitle(dejureTag); 
      }
    }
    if (characterToEuCountryMap[best]) continue;        

    // Doesn't have a useful liege. Absorb him into something nearby.
    Logger::logStream(Logger::Warning) << "Warning: Could not assign CK tag " << primaryTitle->getKey() << ".\n"; 
  }
}

void WorkerThread::createProvinceMap () {
  map<string, int> counts;
  for (objiter link = provinceLinks.begin(); link != provinceLinks.end(); ++link) {
    objvec ckps = (*link)->getValue("ck2");
    if (0 == ckps.size()) continue; 
    objvec currProvs; 
    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
      string cktag = (*ckp)->getLeaf();
      Object* ckprov = ck2Game->safeGetObject(cktag);
      if (!ckprov) {
	Logger::logStream(Logger::Warning) << "Warning: Could not find CK2 province " << cktag << ".\n";
	continue;
      }
      
      if (0 == counts[cktag]) ck2provs.push_back(ckprov);
      currProvs.push_back(ckprov); 
      counts[cktag]++;
    }

    objvec eups = (*link)->getValue("eux");
    for (objiter eup = eups.begin(); eup != eups.end(); ++eup) {
      string eutag = (*eup)->getLeaf();
      Object* euprov = euxGame->safeGetObject(eutag);
      if (!euprov) {
	Logger::logStream(Logger::Warning) << "Warning: Could not find EU province " << eutag << ".\n";
	continue;	
      }

      for (objiter ckp = currProvs.begin(); ckp != currProvs.end(); ++ckp) {
	ckProvToEuProvsMap[*ckp].push_back(euprov);
	euProvToCkProvsMap[euprov].push_back(*ckp);

	if (ckCountyToCkProvinceMap[*ckp]) continue;
	string titletag = remQuotes((*ckp)->safeGetString("title"));
	Object* title = getTitle(titletag);
	if (!title) continue;
	if (County != titleTier(title)) continue;
	ckCountyToCkProvinceMap[title] = (*ckp); 
      }
    }
  }

  for (objiter ckp = ck2provs.begin(); ckp != ck2provs.end(); ++ckp) {
    objvec baronies = (*ckp)->getLeaves();
    for (objiter b = baronies.begin(); b != baronies.end(); ++b) {
      string htype = (*b)->safeGetString("type");
      if (!ckBuildings->safeGetObject(htype)) continue;
      if ((*b)->getKey() == "settlement_construction") continue; 
      Object* btitle = getTitle((*b)->getKey());
      if (!btitle) {
	Logger::logStream(Logger::Warning) << "Warning: " << (*b)->getKey()
					   << " should be a title, but isn't.\n"; 
	continue;
      }
      titleToCkProvinceMap[btitle] = (*ckp); 
    }

    string tag = remQuotes((*ckp)->safeGetString("title"));
    Object* ctitle = getTitle(tag);
    if (!ctitle) {
	Logger::logStream(Logger::Warning) << "Warning: " << tag
					   << " should be a title, but isn't.\n"; 
	continue;
    }
    titleToCkProvinceMap[ctitle] = (*ckp); 
  }
}

void WorkerThread::createVassalsMap () {
  for (objiter cktitle = begin(Titles); cktitle != final(Titles); ++cktitle) {
    string ck2tag = (*cktitle)->getKey();

    // No warning here - just indicates that the title doesn't exist, eg uncreated empire. 
    if ((*cktitle)->safeGetString("holder", "NONE") == "NONE") continue; 
    
    string liegeTag = remQuotes((*cktitle)->safeGetString("liege", "\"NONE\""));
    Object* liegeObj = ck2Game->safeGetObject(liegeTag);
    if (liegeObj) {
      liegeMap[(*cktitle)] = liegeObj;
      vassalMap[liegeObj].push_back((*cktitle));
      Logger::logStream(DebugCountries) << ck2tag << " is vassal of " << liegeTag << ".\n";
    }
    else {
      sovereigns.push_back((*cktitle));
      Logger::logStream(DebugCountries) << ck2tag << " is sovereign.\n";
    }
  }
}

void WorkerThread::initialiseCharacters () {
  static bool done = false;
  if (done) return;
  done = true;
  
  Object* charObject = ck2Game->safeGetObject("character");
  assert(charObject);
  characters = charObject->getLeaves();
  for (objiter c = characters.begin(); c != characters.end(); ++c) {
    int index = atoi((*c)->getKey().c_str());
    assert(!characterIndex[index]);
    characterIndex[index] = (*c); 
  }
  Logger::logStream(Logger::Game) << "Found " << (int) characters.size() << " characters.\n";
      
  objvec allObjects = ck2Game->getLeaves();
  for (objiter ob = allObjects.begin(); ob != allObjects.end(); ++ob) {
    if ((*ob)->safeGetString("succession") == "") continue;
    titles.push_back(*ob);
    titleMap[(*ob)->getKey()] = (*ob); 
  }
  Logger::logStream(Logger::Game) << "Found " << (int) titles.size() << " titles.\n";
}

void WorkerThread::initialiseRelationMaps () {
  static bool done = false;
  if (done) return;
  done = true;
  
  for (objiter curr = begin(Chars); curr != final(Chars); ++curr) {
    Object* father = getChar((*curr)->safeGetString("father"));
    Object* mother = getChar((*curr)->safeGetString("mother"));
    bool female    = ((*curr)->safeGetString("female", "no") == "yes");
    bool living    = ((*curr)->safeGetString("death_date") == "");
    bool married   = (((*curr)->safeGetString("spouse") != "") || ((*curr)->safeGetString("betrothal") != ""));
      
    mothers[*curr].push_back(mother);
    fathers[*curr].push_back(father);
    if (father) children[father].push_back(*curr);
    if (mother) children[mother].push_back(*curr);
    if (female) {
      if (father) {
	daughters[father].push_back(*curr);
	if ((living) && (married)) father->resetLeaf("numMarriedDaughters", father->safeGetInt("numMarriedDaughters") + 1); 
      }
      if (mother) daughters[mother].push_back(*curr);
	
    }
    else {
      if (father) {
	sons[father].push_back(*curr);
	father->resetLeaf("numSons", father->safeGetInt("numSons") + 1);
	if (living) father->resetLeaf("numLivingSons", father->safeGetInt("numLivingSons") + 1);
      }
      if (mother) sons[mother].push_back(*curr);	  
    }
  }

  for (objiter title = begin(Titles); title != final(Titles); ++title) {
    string holderId = (*title)->safeGetString("holder");
    Object* holder = getChar(holderId);
    if (!holder) {
      continue; 
    }
    charTitles[holder].push_back(*title);
    titleToCharMap[*title] = holder; 
  }
}

void WorkerThread::loadFiles () {
  Object* countries = loadTextFile(targetVersion + "country_mappings.txt"); 
  countryLinks = countries->getValue("link");
  Logger::logStream(DebugCountries) << "Found " << countryLinks.size() << " country links.\n";

  countries = loadTextFile(targetVersion + "province_mappings.txt");
  provinceLinks = countries->getValue("link");
  Logger::logStream(DebugCountries) << "Found " << provinceLinks.size() << " province links.\n";

  ckBuildings = loadTextFile(sourceVersion + "buildings.txt");
  staticMods  = loadTextFile(sourceVersion + "static_mods.txt");


  Object* rMap = loadTextFile(targetVersion + "religion_mappings.txt");
  objvec links = rMap->getValue("link"); 
  for (objiter link = links.begin(); link != links.end(); ++link) {
    religionMap[(*link)->safeGetString("ck2")] = (*link)->safeGetString("eux");
  }

  Object* cMap = loadTextFile(targetVersion + "culture_mappings.txt");
  links = cMap->getValue("link"); 
  for (objiter link = links.begin(); link != links.end(); ++link) {
    string special = (*link)->safeGetString("de_jure", "NUFFINK");
    if (special == "NUFFINK") cultureMap[(*link)->safeGetString("ck2")] = (*link)->safeGetString("eux");
    else specialCultureMap[special][(*link)->safeGetString("ck2")] = (*link)->safeGetString("eux");
  }

  Logger::logStream(DebugCulture) << "Found " << cultureMap.size() << " regular and "
				  << specialCultureMap.size() << " special culture conversions.\n";

  Object* traitObj = loadTextFile(sourceVersion + "traits.txt");
  traits = traitObj->getLeaves();
  // CK uses Fortran indexing for the traits
  traits.insert(traits.begin(), new Object("dummyTrait"));

  Object* dynObj = loadTextFile(sourceVersion + "dynasties.txt");
  objvec dyns = dynObj->getLeaves();
  for (objiter dyn = dyns.begin(); dyn != dyns.end(); ++dyn) {
    dynasties[(*dyn)->getKey()] = (*dyn);
    Logger::logStream(DebugLeaders) << "Found dynasty " << (*dyn)->getKey() << ", " << (*dyn)->safeGetString("name") << ".\n"; 
  }
  dyns.clear(); 
  dynObj = ck2Game->safeGetObject("dynasties");
  if (dynObj) dyns = dynObj->getLeaves();
  for (objiter dyn = dyns.begin(); dyn != dyns.end(); ++dyn) {
    if (dynasties[(*dyn)->getKey()]) continue; 
    dynasties[(*dyn)->getKey()] = (*dyn);
    Logger::logStream(DebugLeaders) << "Found dynasty " << (*dyn)->getKey() << ", " << (*dyn)->safeGetString("name") << ".\n"; 
  }
}


/******************************* End initialisers *******************************/ 

/******************************* Begin EU3 conversions ********************************/

void WorkerThread::eu3Armies () {
  int unitId = euxGame->safeGetInt("unit"); 
  
  int euShips = 0;
  vector<string> harbours;
  map<Object*, vector<string> > shipNames; 
  Object* forbidShips = configObject->safeGetObject("forbidShips");
  vector<string> inlands;
  if (forbidShips) {
    for (int i = 0; i < forbidShips->numTokens(); ++i) {
      inlands.push_back(forbidShips->getToken(i)); 
    }
  }
  
  // Loop for armies
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euNation = (*i).first;    
    Object* ckRuler = (*i).second;

    objvec navies = euNation->getValue("navy");
    for (objiter navy = navies.begin(); navy != navies.end(); ++navy) {
      objvec ships = (*navy)->getValue("ship");
      euShips += ships.size();
      string provTag = (*navy)->safeGetString("location");
      if (find(inlands.begin(), inlands.end(), provTag) == inlands.end()) harbours.push_back(provTag);
      for (objiter ship = ships.begin(); ship != ships.end(); ++ship) {
	shipNames[euNation].push_back((*ship)->safeGetString("name"));
      } 
    }
    
    euNation->unsetValue("army");
    Object* demesne = ckRuler->safeGetObject("demesne");
    if (!demesne) continue;

    int retinue = 0;
    string euLocation = ""; 
    objvec ckArmies = demesne->getValue("army"); 
    for (objiter ckArmy = ckArmies.begin(); ckArmy != ckArmies.end(); ++ckArmy) {
      objvec units = (*ckArmy)->getValue("sub_unit");
      for (objiter unit = units.begin(); unit != units.end(); ++unit) {
	if ((*unit)->safeGetString("retinue_type", "BLAH") == "BLAH") continue;
	retinue++;
      }
      if (euLocation != "") continue; 
      string ckTag = (*ckArmy)->safeGetString("location");
      Object* ckProv = ck2Game->safeGetObject(ckTag);
      if (!ckProv) continue;
      objvec euProvs = ckProvToEuProvsMap[ckProv];
      for (objiter cand = euProvs.begin(); cand != euProvs.end(); ++cand) {
	if (euNation->getKey() != remQuotes((*cand)->safeGetString("owner"))) continue;
	euLocation = (*cand)->getKey();
	break; 
      }
    }

    if (0 == retinue) continue; // Rhyming code!
    if (euLocation == "") euLocation = euNation->safeGetString("capital"); 

    Logger::logStream(DebugUnits) << "Creating " << retinue
				  << " regiments for tag " << euNation->getKey()
				  << ", to be placed in " << euLocation << ".\n"; 
    
    Object* euArmy = new Object("army");
    euNation->setValue(euArmy); 
    Object* id = euArmy->getNeededObject("id");
    id->setLeaf("id", unitId++);
    id->setLeaf("type", "43");

    string armyname = "Retinue of ";
    armyname += remQuotes(ckRuler->safeGetString("birth_name"));
    euArmy->setLeaf("name", addQuotes(armyname));
    euArmy->setLeaf("location", euLocation);
    for (int reg = 0; reg < retinue; ++reg) {
      Object* regiment = new Object("regiment");
      euArmy->setValue(regiment);
      id = regiment->getNeededObject("id");
      id->setLeaf("id", unitId++);
      id->setLeaf("type", "43");
      sprintf(stringbuffer, "\"Retinue regiment %i\"", reg+1);
      regiment->setLeaf("name", stringbuffer);
      regiment->setLeaf("home", euLocation);
      regiment->setLeaf("type", "\"western_medieval_infantry\"");
      regiment->setLeaf("strength", "1.000"); 
    }
  }

  // Loop for navies, which are handled differently.
  int ckShips = 1;
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euNation = (*i).first;    
    Object* ckRuler = (*i).second;
    string euLocation = "";   
    
    euNation->unsetValue("navy");
    int currentShips = 0;
    
    for (objiter ckp = euCountryToCkProvincesMap[euNation].begin(); ckp != euCountryToCkProvincesMap[euNation].end(); ++ckp) {
      Object* province = (*ckp);
      
      objvec baronies = province->getLeaves();
      bool coastal = false; 
      for (objiter barony = baronies.begin(); barony != baronies.end(); ++barony) {
      	string type = (*barony)->safeGetString("type", "BLAH");
	if (type == "BLAH") continue;

	// Do not use getCkWeight here because castles have a base galley weight,
	// so we can't distinguish coastal provinces that way. 
	Object* buildList = ckBuildings->safeGetObject(type);
	if (buildList) { 
	  objvec buildings = buildList->getLeaves();
	  for (objiter building = buildings.begin(); building != buildings.end(); ++building) {
	    if ((*barony)->safeGetString((*building)->getKey(), "no") != "yes") continue;
	    int galleys = (*building)->safeGetInt("galleys");
	    if (0 == galleys) continue;
	    currentShips += galleys;
	    coastal = true; 
	  }
	}
      }
    
      if (!coastal) continue;
      shipNames[euNation].push_back(province->safeGetString("name")); 
      if (euLocation != "") continue; 
      objvec euProvs = ckProvToEuProvsMap[province];
      for (objiter cand = euProvs.begin(); cand != euProvs.end(); ++cand) {
	if (euNation->getKey() != remQuotes((*cand)->safeGetString("owner"))) continue;
	if (find(inlands.begin(), inlands.end(), (*cand)->getKey()) != inlands.end()) continue; 
	if ((euLocation == "") ||
	    (((find(harbours.begin(), harbours.end(), euLocation) != harbours.end()) &&
	      (find(harbours.begin(), harbours.end(), (*cand)->getKey()) != harbours.end()))))
	  euLocation = (*cand)->getKey();
	break; 
      }
    }
  
    if (euLocation == "") {
      if (find(harbours.begin(), harbours.end(), euNation->safeGetString("capital")) != harbours.end()) euLocation = euNation->safeGetString("capital");
      else if (0 < harbours.size()) euLocation = harbours[0];
      else euLocation = "1"; 
    }
    ckRuler->resetLeaf("totalShips", currentShips);
    ckRuler->resetLeaf("navyLocation", euLocation); 
    ckShips += currentShips;
    if (0 < currentShips) Logger::logStream(DebugUnits) << "Found " << currentShips << " CK galleys for tag " << euNation->getKey()
							<< ", with navy location " << euLocation << ".\n"; 
  }

  Logger::logStream(DebugUnits) << "Found " << ckShips << " CK ships and " << euShips << " EU ships.\n"; 
  
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euNation = (*i).first;    
    Object* ckRuler = (*i).second;

    double currentShips = ckRuler->safeGetFloat("totalShips");
    currentShips /= ckShips;
    currentShips *= euShips;
    
    int shipsToCreate = (int) floor(currentShips + 0.5);
    if (0 == shipsToCreate) continue;
    Logger::logStream(DebugUnits) << "Creating " << shipsToCreate
				  << " EU ships for tag "
				  << euNation->getKey() << "\n"; 
    int namesUsed = 0; 

    Object* euNavy = new Object("navy");
    euNation->setValue(euNavy);
    Object* id = euNavy->getNeededObject("id");
    id->setLeaf("id", unitId++);
    id->setLeaf("type", "43");
    sprintf(stringbuffer, "\"%s's Navy\"", remQuotes(ckRuler->safeGetString("birth_name")).c_str());
    euNavy->setLeaf("name", stringbuffer); 
    euNavy->setLeaf("location", ckRuler->safeGetString("navyLocation"));
    //euNavy->setLeaf("location", "1");
    for (int s = 0; s < shipsToCreate; ++s) {
      Object* ship = new Object("ship");
      euNavy->setValue(ship);
      id = ship->getNeededObject("id");
      id->setLeaf("id", unitId++);
      id->setLeaf("type", "43");
      if (0 == shipNames[euNation].size()) ship->setLeaf("name", "\"Ship\"");
      else {
	ship->setLeaf("name", shipNames[euNation][namesUsed++]);
	if (namesUsed >= (int) shipNames[euNation].size()) namesUsed = 0;
      }
      ship->setLeaf("home", ckRuler->safeGetString("navyLocation"));
      //ship->setLeaf("home", "1"); 
      ship->setLeaf("type", 0 == s%2 ? "\"carrack\"" : "\"cog\"");
      ship->setLeaf("strength", "1.000"); 
    }
  }

  
  euxGame->resetLeaf("unit", unitId); 
}

void WorkerThread::eu3Cores () {
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    objvec ckps = (*link).second;
    if (0 == ckps.size()) continue; // Already gave a warning about this.

    eup->unsetValue("core");
    Object* history = eup->getNeededObject("history");
    history->unsetValue("add_core");

    // For each CK province, look up who owns it de-jure and give him (or them) a core on it.
    set<Object*> dejures;    
    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
      string titleTag = remQuotes((*ckp)->safeGetString("title"));
      Object* title = getTitle(titleTag);
      while (title) {
	dejures.insert(title);
	titleTag = remQuotes(title->safeGetString("de_jure_liege"));
	Object* liege = getTitle(titleTag);

	if (!liege) { // Either independent, or currently vassal to the de-jure liege.
	  titleTag = remQuotes(title->safeGetString("liege"));
	  liege = getTitle(titleTag);
	}
	title = liege; 
      }
    }

    map<Object*, int> gotCores; 
    for (set<Object*>::iterator title = dejures.begin(); title != dejures.end(); ++title) {
      Object* holder = titleToCharMap[*title];
      if (!holder) continue;
      Object* euNation = characterToEuCountryMap[holder];
      if (!euNation) continue;
      if (gotCores[euNation] > 0) continue; 
      
      // Check if this title is primary-level, ie empire for emperors, etc.
      TitleTier level = titleTier(*title);
      bool primary = true;      
      for (objiter i = beginRel(holder, Title); i != finalRel(holder, Title); ++i) {
	if (level >= titleTier(*i)) continue;
	primary = false;
	break;
      }
      if (!primary) continue;
      
      Logger::logStream(DebugCores) << "Giving core on "
				    << nameAndNumber(eup)
				    << " to tag " 
				    << euNation->getKey()
				    << " due to de-jure title "
				    << (*title)->getKey()
				    << ".\n";
      eup->setLeaf("core", addQuotes(euNation->getKey()));
      history->setLeaf("add_core", addQuotes(euNation->getKey()));
      gotCores[euNation]++; 
    }
    
  }
}

void WorkerThread::eu3Cots () {
  Object* trade = euxGame->getNeededObject("trade");
  trade->clear();
  Object* diplomacy = euxGame->getNeededObject("diplomacy");
  map<string, map<string, bool> > openMarkets; 
  for (map<Object*, Object*>::iterator i = euCountryToCkCountryMap.begin(); i != euCountryToCkCountryMap.end(); ++i) {
    Object* euCountry = (*i).first;

    if (euCountry->safeGetString("government") != "merchant_republic") continue;
    string location = euCountry->safeGetString("capital");
    Object* euProv = euxGame->safeGetObject(location);
    if (!euProv) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find province " << location
					 << ", alleged capital of " << euCountry->getKey()
					 << "; no COT placed.\n";
      continue;
    }
    if (remQuotes(euProv->safeGetString("owner")) != euCountry->getKey()) {
      Logger::logStream(Logger::Warning) << "Warning: Province " << location
					 << ", alleged capital of " << euCountry->getKey()
					 << ", is owned by " << euProv->safeGetString("owner")
					 << ". No COT placed.\n";
      continue;
    }

    Object* cot = new Object("cot");
    trade->setValue(cot);
    cot->setLeaf("location", location);
    Object* owner = new Object(euCountry->getKey());
    cot->setValue(owner);
    owner->setLeaf("level", "5");

    Logger::logStream(DebugCots) << "Creating COT in " << location
				 << " due to merchant republic "
				 << euCountry->getKey() << ".\n"; 

    map<string, double> posts;
    double totalPosts = 0; 
    Object* doge = euCountryToCharacterMap[euCountry]; 
    for (objiter ckp = ck2provs.begin(); ckp != ck2provs.end(); ++ckp) {
      Object* tradepost = (*ckp)->safeGetObject("tradepost");
      if (!tradepost) continue;
      string holder = tradepost->safeGetString("owner");
      Object* ckRuler = getChar(holder);
      if (ckRuler != doge) continue;

      for (objiter eup = ckProvToEuProvsMap[*ckp].begin(); eup != ckProvToEuProvsMap[*ckp].end(); ++eup) {
	string ownerTag = remQuotes((*eup)->safeGetString("owner"));
	if (ownerTag == euCountry->getKey()) continue;
	posts[ownerTag]++;
	totalPosts++; 
      }
    }
    for (map<string, double>::iterator p = posts.begin(); p != posts.end(); ++p) {
      double merchants = (*p).second;
      merchants /= totalPosts;
      merchants *= configObject->safeGetFloat("numForeignMerchants", 10);
      if (merchants < 0.5) continue;
      if (merchants > 5) merchants = 5;

      string merchTag = (*p).first; 
      Logger::logStream(DebugCots) << "  Assigning " << (int) floor(0.5 + merchants) << " merchants from tag "
				   << merchTag << ".\n"; 
      
      owner = new Object(merchTag);
      cot->setValue(owner);
      owner->resetLeaf("level", (int) floor(merchants + 0.5));

      if (openMarkets[euCountry->getKey()][merchTag]) continue;
      openMarkets[euCountry->getKey()][merchTag] = true;
      Object* marketOpen = new Object("open_market");
      diplomacy->setValue(marketOpen);
      marketOpen->setLeaf("first", addQuotes(euCountry->getKey()));
      marketOpen->setLeaf("second", addQuotes(merchTag));
      marketOpen->setLeaf("start_date", "\"1.1.1\""); 
    }
  }
}

void WorkerThread::eu3Characters () {
  int monarchId = 1;
  Object* dummyWorstChar = new Object("dummyWorst");
  Object* dummyBestChar  = new Object("dummyBest"); 
  
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    if ((euCountry->getKey() == "PIR") || (euCountry->getKey() == "REB")) continue;
    Object* ckRuler = (*i).second;
    calculateAttributes(ckRuler);
    for (unsigned int a = 0; a < attribNames.size(); ++a) {
      if (ckRuler->safeGetInt(attribNames[a]) < dummyWorstChar->safeGetInt(attribNames[a]))
	dummyWorstChar->resetLeaf(attribNames[a], ckRuler->safeGetInt(attribNames[a]));
      else if (ckRuler->safeGetInt(attribNames[a]) > dummyBestChar->safeGetInt(attribNames[a]))
	dummyBestChar->resetLeaf(attribNames[a], ckRuler->safeGetInt(attribNames[a]));
    }
  }
  
  for (unsigned int a = 0; a < attribNames.size(); ++a) {
    dummyBestChar->resetLeaf(attribNames[a], dummyBestChar->safeGetInt(attribNames[a]) - dummyWorstChar->safeGetFloat(attribNames[a])); 
  }
    
  // Convert monarchs and heirs.
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    if ((euCountry->getKey() == "PIR") || (euCountry->getKey() == "REB")) continue; 
    
    Object* ckRuler    = (*i).second;
    Object* euHistory  = euCountry->getNeededObject("history"); 
    Object* rulerEvent = new Object(remQuotes(ckRuler->safeGetString("birth_date", "1399.1.1")));
    Object* euMonarch  = new Object("monarch");

    rulerEvent->setValue(euMonarch);
    euHistory->setValue(rulerEvent);
    setCharacterAttributes(euMonarch, ckRuler, monarchId, dummyBestChar, dummyWorstChar);
    
    Object* nationPointer = euCountry->getNeededObject("monarch");
    nationPointer->resetLeaf("id", monarchId);
    nationPointer->resetLeaf("type", "37");
    monarchId++;

    Object* ckHeir = 0; 
    for (objiter son = beginRel(ckRuler, Son); son != finalRel(ckRuler, Son); ++son) {
      if ((*son)->safeGetString("death_date", "ILIVE") != "ILIVE") continue;
      ckHeir = (*son);
      break;
    }

    if (!ckHeir) {
      for (objiter son = beginRel(ckRuler, Daughter); son != finalRel(ckRuler, Daughter); ++son) {
	if ((*son)->safeGetString("death_date", "ILIVE") != "ILIVE") continue;
	ckHeir = (*son);
	break;
      }
    }
      
    if (!ckHeir) {
      euCountry->unsetValue("heir");
      continue;
    }

    Object* euHeir = new Object("heir");
    rulerEvent = new Object(remQuotes(ckHeir->safeGetString("birth_date", "1399.1.1")));
    rulerEvent->setValue(euHeir);
    euHistory->setValue(rulerEvent);
    setCharacterAttributes(euHeir, ckHeir, monarchId, dummyBestChar, dummyWorstChar);
    euHeir->setLeaf("birth_date", ckHeir->safeGetString("birth_date", "1399.1.1")); 
    
    nationPointer = euCountry->getNeededObject("heir");
    nationPointer->resetLeaf("id", monarchId);
    nationPointer->resetLeaf("type", "37");
    monarchId++;
  }

  euxGame->resetLeaf("monarch", monarchId);

  // Generals and advisors
  int leaderId = euxGame->safeGetInt("leader");
  int advisorId = euxGame->safeGetInt("advisor");
  Object* advisorList = euxGame->getNeededObject("active_advisors");
  
  advisorList = advisorList->getNeededObject("western");
  advisorList->clear(); 
  Object* generalTraits = configObject->getNeededObject("general_traits"); 
  for (objiter c = begin(); c != final(); ++c) {
    string job = remQuotes((*c)->safeGetString("job_title", "\"BLAH\""));
    if (job == "BLAH") continue;

    Object* employer = getChar((*c)->safeGetInt("employer"));
    if (!employer) continue;
    Object* euNation = characterToEuCountryMap[employer];
    if (!euNation) continue;
    
    if (job == "job_marshal") {
      // Make a general
      Object* history = euNation->getNeededObject("history");
      Object* event = new Object(remQuotes(euxGame->safeGetString("start_date")));
      history->setValue(event);
      Object* leader = new Object("leader");
      event->setValue(leader);
      leader->setLeaf("name", (*c)->safeGetString("birth_name"));
      leader->setLeaf("type", "general");
      leader->setLeaf("activation", euxGame->safeGetString("start_date")); 
      calculateAttributes(*c);
      int fire = 0;
      int shock = (int) floor(sqrt((*c)->safeGetFloat("martial")) + 0.5); 
      int maneuver = 0;
      int siege = 0;

      Object* charTraits = (*c)->getNeededObject("traits");
      for (int i = 0; i < charTraits->numTokens(); ++i) {
	string trait = traits[charTraits->tokenAsInt(i)]->getKey();
	objvec adds = generalTraits->getValue(trait);
	for (objiter a = adds.begin(); a != adds.end(); ++a) {
	  string add = (*a)->getLeaf(); 
	  if (add == "fire") fire++;
	  else if (add == "maneuver") maneuver++;
	  else if (add == "shock") shock++;
	  else if (add == "siege") siege++;
	}
      }

      Logger::logStream(DebugLeaders) << "Marshal " << (*c)->safeGetString("birth_name")
				      << " becomes " << fire << "-" << shock << "-"
				      << maneuver << "-" << siege << " general for "
				      << euNation->getKey() << ".\n";
      leader->setLeaf("fire", fire);
      leader->setLeaf("shock", shock);
      leader->setLeaf("manuever", maneuver); // Not my typo, Pdox spells it thus.
      leader->setLeaf("siege", siege);
      Object* id = leader->getNeededObject("id");
      id->setLeaf("id", leaderId);
      id->setLeaf("type", "38");

      leader = new Object("leader");
      leader->setLeaf("id", leaderId);
      leader->setLeaf("type", "38");
      euNation->setValue(leader); 
      leaderId++;
    }
    else {
      // Make an advisor
      Object* jobOffers = configObject->safeGetObject(job);
      if (!jobOffers) continue;
      objvec jobs = jobOffers->getLeaves();
      map<Object*, double> points;
      calculateAttributes(*c);
      Object* charTraits = (*c)->getNeededObject("traits");
      double maxPoints = -1;
      Object* bestJob = 0; 
      for (objiter j = jobs.begin(); j != jobs.end(); ++j) {
	objvec cvItems = (*j)->getLeaves();
	for (objiter cv = cvItems.begin(); cv != cvItems.end(); ++cv) {
	  string key = (*cv)->getKey();
	  int attrib = (*c)->safeGetInt(key, -999);
	  if (-999 == attrib) { // This is a trait
	    for (int i = 0; i < charTraits->numTokens(); ++i) {
	      int currTrait = charTraits->tokenAsInt(i);
	      if (key != traits[currTrait]->getKey()) continue; 
	      points[*j] += (*j)->safeGetFloat(key);
	      break; 
	    }
	  }
	  else {
	    points[*j] += attrib * (*j)->safeGetFloat(key);
	  }
	}
	if (points[*j] < maxPoints) continue;
	maxPoints = points[*j];
	bestJob = (*j); 
      }

      if (!bestJob) continue;
      Object* province = euxGame->safeGetObject(euNation->safeGetString("capital"));
      if ((!province) || (remQuotes(province->safeGetString("owner")) != euNation->getKey())) {
	for (map<Object*, objvec>::iterator p = euProvToCkProvsMap.begin(); p != euProvToCkProvsMap.end(); ++p) {
	  Object* prov = (*p).first;
	  if (remQuotes(prov->safeGetString("owner")) != euNation->getKey()) continue;
	  province = prov;
	  break;
	}
      }
      if (!province) break; 

      if (maxPoints > 36) maxPoints = 36;
      if (maxPoints < 1) maxPoints = 1;
      int skill = (int) floor(sqrt(maxPoints + 0.5));
      Logger::logStream(DebugLeaders) << "Making " << job << " " << (*c)->safeGetString("birth_name")
				      << " a " << bestJob->getKey() << " of skill " << skill << " for tag " << euNation->getKey() << ".\n";

      Object* history = province->getNeededObject("history"); 
      Object* event = new Object(remQuotes(euxGame->safeGetString("start_date")));
      history->setValue(event);
      Object* advisor = new Object("advisor");
      event->setValue(advisor);
      advisor->setLeaf("name", (*c)->safeGetString("birth_name"));
      advisor->setLeaf("type", bestJob->getKey());
      advisor->setLeaf("date", euxGame->safeGetString("start_date"));
      advisor->setLeaf("hire_date", "\"1.1.1\"");
      advisor->setLeaf("home", addQuotes(euNation->getKey()));
      advisor->setLeaf("location", province->getKey());
      advisor->setLeaf("skill", skill); 
      Object* id = advisor->getNeededObject("id");
      id->setLeaf("id", advisorId);
      id->setLeaf("type", "39");
      id = new Object("advisor");
      advisorList->setValue(id);
      id->setLeaf("id", advisorId);
      id->setLeaf("type", "39");
      advisorId++; 
    }
  }

  euxGame->resetLeaf("leader", leaderId);
  euxGame->resetLeaf("advisor", advisorId);   
}

void WorkerThread::eu3Diplomacy () {
  Object* eu3Diplomacy = euxGame->getNeededObject("diplomacy");
  eu3Diplomacy->clear(); 

  for (map<Object*, Object*>::iterator i = euCountryToCkCountryMap.begin(); i != euCountryToCkCountryMap.end(); ++i) {
    Object* euCountry = (*i).first;
    Object* ckCountry = (*i).second;

    Object* liege = liegeMap[ckCountry];
    if (liege) liege = titleToCharMap[liege];
    if (liege) liege = characterToEuCountryMap[liege];
    if ((liege) && (liege != euCountry)) {
      Object* vassal = new Object("vassal");
      eu3Diplomacy->setValue(vassal);
      vassal->setLeaf("first",  addQuotes(liege->getKey()));
      vassal->setLeaf("second", addQuotes(euCountry->getKey()));
      vassal->setLeaf("start_date", "1399.1.1");
    }
  }

  objvec existingWars = euxGame->getValue("active_war");
  for (objiter war = existingWars.begin(); war != existingWars.end(); ++war) {
    objvec attackers = (*war)->getValue("attacker");
    objvec defenders = (*war)->getValue("defender");
    bool remove = false;
    for (objiter attack = attackers.begin(); attack != attackers.end(); ++attack) {
      string tag = remQuotes((*attack)->getLeaf());
      Object* att = euxGame->safeGetObject(tag);
      if (!att) {
	Logger::logStream(Logger::Warning) << "Warning: War " << (*war)->safeGetString("name")
					   << " contains nonexistent tag " << tag
					   << ". Suggest fixing this in input save.\n";
	continue; 
      }
      if (euCountryToCkCountryMap.find(att) != euCountryToCkCountryMap.end()) remove = true; 
    }
    for (objiter defend = defenders.begin(); defend != defenders.end(); ++defend) {
      string tag = remQuotes((*defend)->getLeaf());
      Object* att = euxGame->safeGetObject(tag);
      if (!att) {
	Logger::logStream(Logger::Warning) << "Warning: War " << (*war)->safeGetString("name")
					   << " contains nonexistent tag " << tag
					   << ". Suggest fixing this in input save.\n";
	continue; 
      }
      if (euCountryToCkCountryMap.find(att) != euCountryToCkCountryMap.end()) remove = true;       
    }
    if (!remove) continue;
    euxGame->removeObject(*war);
  }


  objvec ckwars = ck2Game->getValue("active_war");
  for (objiter war = ckwars.begin(); war != ckwars.end(); ++war) {
    objvec ckAttackers = (*war)->getValue("attacker");
    objvec ckDefenders = (*war)->getValue("defender");
    objvec euAttackers;
    objvec euDefenders;

    for (objiter cka = ckAttackers.begin(); cka != ckAttackers.end(); ++cka) {
      Object* ckRuler = getChar((*cka)->getLeaf());
      Object* euCountry = characterToEuCountryMap[ckRuler];
      if (!euCountry) continue;
      if (euCountryToCharacterMap[euCountry] != ckRuler) continue;       
      if (find(euAttackers.begin(), euAttackers.end(), euCountry) != euAttackers.end()) continue;
      euAttackers.push_back(euCountry); 
    }
    for (objiter cka = ckDefenders.begin(); cka != ckDefenders.end(); ++cka) {
      Object* ckRuler = getChar((*cka)->getLeaf());
      Object* euCountry = characterToEuCountryMap[ckRuler];
      if (!euCountry) continue;
      if (euCountryToCharacterMap[euCountry] != ckRuler) continue; // Check for sovereignty
      if (find(euDefenders.begin(), euDefenders.end(), euCountry) != euDefenders.end()) continue;
      euDefenders.push_back(euCountry); 
    }

    if (0 == euAttackers.size() * euDefenders.size()) {
      Logger::logStream(DebugDiplomacy) << "Skipping CK war "
					<< (*war)->safeGetString("name")
					<< " because one side didn't convert.\n";
      continue;
    }

    Object* euWar = new Object("active_war");
    euxGame->setValue(euWar);
    euWar->setLeaf("name", (*war)->safeGetString("name"));
    for (objiter eua = euAttackers.begin(); eua != euAttackers.end(); ++eua) {
      euWar->setLeaf("attacker", addQuotes((*eua)->getKey()));
      euWar->setLeaf("original_attacker", addQuotes((*eua)->getKey()));
    }
    for (objiter eua = euDefenders.begin(); eua != euDefenders.end(); ++eua) {
      euWar->setLeaf("defender", addQuotes((*eua)->getKey()));
      euWar->setLeaf("original_defender", addQuotes((*eua)->getKey()));
    }
    euWar->setLeaf("action", euxGame->safeGetString("start_date"));
    Logger::logStream(DebugDiplomacy) << "Converting war " << (*war)->safeGetString("name") << ".\n"; 
  }

}

void WorkerThread::eu3Governments () {
  for (map<Object*, Object*>::iterator i = euCountryToCkCountryMap.begin(); i != euCountryToCkCountryMap.end(); ++i) {
    Object* euCountry = (*i).first;
    Object* ckCountry = (*i).second;
    
    string succession = ckCountry->safeGetString("succession", "primogeniture");
    if (succession == "patrician_elective") euCountry->resetLeaf("government", "merchant_republic");
    else euCountry->resetLeaf("government", "feudal_monarchy");

    Logger::logStream(DebugGovernments) << "Government of " << ckCountry->getKey()
					<< " (" << euCountry->getKey() << ") is "
					<< succession << " -> " << euCountry->safeGetString("government")
					<< ".\n"; 
    
    Object* euHistory = euCountry->getNeededObject("history");
    euHistory->resetLeaf("government", euCountry->safeGetString("government")); 
  }  
}

void WorkerThread::eu3Histories () {
  Object* keywords = configObject->getNeededObject("keywords_to_remove");
  objvec provWords = keywords->getValue("province"); 
  
  // Clear histories of provinces and states.
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    Object* euHistory = eup->getNeededObject("history");
    objvec leaves = euHistory->getLeaves();
    for (objiter leaf = leaves.begin(); leaf != leaves.end(); ++leaf) {
      if ((*leaf)->isLeaf()) continue;
      for (objiter word = provWords.begin(); word != provWords.end(); ++word) {
	Object* w = (*leaf)->safeGetObject((*word)->getLeaf());
	if (!w) continue;
	euHistory->removeObject(*leaf); 
      }
    }
  }

  objvec stateWords = keywords->getValue("state"); 
  for (map<Object*, Object*>::iterator i = euCountryToCkCountryMap.begin(); i != euCountryToCkCountryMap.end(); ++i) {
    Object* euCountry = (*i).first;
    Object* euHistory = euCountry->getNeededObject("history");
    objvec leaves = euHistory->getLeaves();
    for (objiter leaf = leaves.begin(); leaf != leaves.end(); ++leaf) {
      if ((*leaf)->isLeaf()) continue;
      for (objiter word = stateWords.begin(); word != stateWords.end(); ++word) {
	Object* w = (*leaf)->safeGetObject((*word)->getLeaf());
	if (!w) continue;
	euHistory->removeObject(*leaf); 
      }
    }    
  }
}

struct EmperorCandidate {
  Object* ckTitle;
  Object* euCountry;
  Object* ckRuler;
  double score; 
};

void WorkerThread::eu3Hre () {
  // States whose capital is in the HRE are members; some are additionally electors, which is a specific leaf.
  // All provinces have already been removed from the HRE in eu3Provinces. Now remove elector states and check
  // for eligibility.

  
  vector<EmperorCandidate*> candidates; 
  for (map<Object*, Object*>::iterator i = euCountryToCkCountryMap.begin(); i != euCountryToCkCountryMap.end(); ++i) {
    Object* euCountry = (*i).first;

    Object* euHistory = euCountry->getNeededObject("history");
    euHistory->unsetValue("elector");
    euHistory->unsetValue("emperor");
    euCountry->unsetValue("elector");
    euCountry->unsetValue("preferred_emperor");

    Object* ckRuler = euCountryToCharacterMap[euCountry];
    if (!ckRuler) continue;

    EmperorCandidate* curr = new EmperorCandidate();
    curr->ckTitle = 0;
    for (objiter title = beginRel(ckRuler, Title); title != finalRel(ckRuler, Title); ++title) {
      if (Empire != titleTier(*title)) continue;
      curr->ckTitle = (*title);
      break;
    }
    if (!curr->ckTitle) continue;

    Logger::logStream(DebugHre) << "Ruler " << ckRuler->safeGetString("birth_name")
				<< " with EU tag " << euCountry->getKey()
				<< " is candidate for HRE due to CK title "
				<< curr->ckTitle->getKey()
				<< ".\n";
    curr->euCountry = euCountry;
    curr->ckRuler = ckRuler;
    candidates.push_back(curr); 
  }

  if (0 == candidates.size()) {
    Logger::logStream(DebugHre) << "No candidates for HRE - dissolving it.\n";
    euxGame->unsetValue("emperor");
    euxGame->unsetValue("old_emperor");
    return; 
  }

  EmperorCandidate* emperor = 0; 
  for (vector<EmperorCandidate*>::iterator c = candidates.begin(); c != candidates.end(); ++c) {
    (*c)->score = getTotalCkWeight((*c)->euCountry); 
    if ((emperor) && (emperor->score > (*c)->score)) continue;
    emperor = (*c); 
  }

  Logger::logStream(DebugHre) << "Candidate " << emperor->ckRuler->safeGetString("birth_name")
			      << " of tag " << emperor->euCountry->getKey() 
			      << " is made emperor with score of " << emperor->score << ".\n";
  if (1 < candidates.size()) {
    Logger::logStream(DebugHre) << "Also ran:\n";
    for (vector<EmperorCandidate*>::iterator c = candidates.begin(); c != candidates.end(); ++c) {
      if (emperor == (*c)) continue; 
      Logger::logStream(DebugHre) << "  " << (*c)->ckRuler->safeGetString("birth_name") << " of "
				  << (*c)->euCountry->getKey() << " with score "
				  << (*c)->score << "\n"; 
    }
  }

  objvec electors;
  objvec done;
  recursiveAddToHre(emperor->ckRuler, emperor->euCountry, electors, done);
  Object* history = emperor->euCountry->getNeededObject("history");
  history->resetLeaf("emperor", "yes"); 
  euxGame->resetLeaf("emperor", addQuotes(emperor->euCountry->getKey()));
  
  for (objiter elector = electors.begin(); elector != electors.end(); ++elector) {
    Logger::logStream(DebugHre) << "Making " << (*elector)->getKey() << " an elector due to score "
				<< (*elector)->safeGetString("score")
				<< ".\n";
    (*elector)->unsetValue("score"); 
    (*elector)->setLeaf("elector", "yes");
    (*elector)->setLeaf("preferred_emperor", addQuotes((*elector)->getKey())); 
    history = (*elector)->getNeededObject("history");
    history->setLeaf("elector", "yes");
  }
  
}

void WorkerThread::eu3Manpower () {
  double totalEuMp = 0;
  double totalCkMp = 0; 
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    totalEuMp += (*link).first->safeGetFloat("manpower");
    for (objiter ckp = (*link).second.begin(); ckp != (*link).second.end(); ++ckp) {
      totalCkMp += getCkWeight(*ckp);
    }    
  }

  Logger::logStream(DebugManpower) << "Total manpower before redistribution: " << totalEuMp << "\n";
  
  double overflow = 0;
  double afterTotal = 0;
  map<string, pair<double, double> > gains;   
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first; 
    
    double currWeight = 0; 
    for (objiter ckp = (*link).second.begin(); ckp != (*link).second.end(); ++ckp) {
      currWeight += getCkWeight(*ckp);
    }
    currWeight /= totalCkMp;
    currWeight *= totalEuMp;

    currWeight += overflow;
    double manpower = floor(currWeight + 0.5);
    overflow = (currWeight - manpower);

    double inputValue = eup->safeGetFloat("manpower");
    Logger::logStream(DebugManpower) << "Province " << nameAndNumber(eup) << " has initial MP " << inputValue
				    << ", final MP " << manpower << "\n";
    gains[eup->safeGetString("owner")].first  += inputValue;
    gains[eup->safeGetString("owner")].second += manpower;
    
    afterTotal += manpower; 
    eup->resetLeaf("manpower", manpower);
    Object* history = eup->getNeededObject("history");
    history->resetLeaf("manpower", manpower); 
  }

  Logger::logStream(DebugManpower) << "After distribution: " << afterTotal << "\n"
				   << "Gains and losses (not showing changes of 9 or less):\n"
				   << "Tag\tInitial\tFinal\tChange\n"; 
  for (map<string, pair<double, double> >::iterator gain = gains.begin(); gain != gains.end(); ++gain) {
    double change = (*gain).second.second - (*gain).second.first; 
    if (fabs(change) < 9.1) continue; 
    Logger::logStream(DebugManpower) << remQuotes((*gain).first) << "\t"
				     << (*gain).second.first << "\t"
				     << (*gain).second.second << "\t"
				     << change 
				     << "\n"; 
  }
}

void WorkerThread::eu3Papacy () {
  Object* papacy = euxGame->safeGetObject("papacy");
  if (!papacy) {
    Logger::logStream(Logger::Warning) << "Warning: Could not find papacy, skipping.\n"; 
    return;
  }

  Object* papalController = 0;
  Object* papalNation = 0;
  double maxPiety = 0.01; 
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* ckRuler = (*i).second;
    Object* euNation = (*i).first; 
    if (remQuotes(ckRuler->safeGetString("religion")) != "catholic") {
      euNation->unsetValue("papal_influence"); 
      continue;
    }

    double piety = ckRuler->safeGetFloat("piety");
    if ((!papalController) || (piety > papalController->safeGetFloat("piety"))) papalController = ckRuler;
    else continue;
    papalNation = euNation;
    maxPiety = piety; 
  }

  Logger::logStream(DebugReligion) << papalController->safeGetString("birth_name")
				   << " with " << maxPiety
				   << " is the most pious Catholic, setting "
				   << papalNation->getKey()
				   << " as controller.\n";
  papacy->resetLeaf("controller", addQuotes(papalNation->getKey()));

  double maxInfluence = configObject->safeGetFloat("maxPapalInfluence"); 
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* ckRuler = (*i).second;
    Object* euNation = (*i).first; 
    if (remQuotes(ckRuler->safeGetString("religion")) != "catholic") continue;

    double piety = ckRuler->safeGetFloat("piety");
    piety /= maxPiety;
    piety *= maxInfluence; 
    
    Logger::logStream(DebugReligion) << euNation->getKey() << " has piety "
				     << ckRuler->safeGetString("piety")
				     << " giving papal influence "
				     << piety << ".\n";
    euNation->resetLeaf("papal_influence", piety); 

  }
  
  objvec bishops;
  objvec euProvs; 
  for (map<Object*, Object*>::iterator i = titleToCkProvinceMap.begin(); i != titleToCkProvinceMap.end(); ++i) {
    Object* ckProvince = (*i).second;
    if (!ckProvince) continue;
    objvec holdings = ckProvince->getLeaves();
    for (objiter holding = holdings.begin(); holding != holdings.end(); ++holding) {
      if ((*holding)->safeGetString("type") != "temple") continue;
      if ((*holding)->getKey() == "settlement_construction") continue; 
      Object* ckTitle = getTitle((*holding)->getKey());

      if (!ckTitle) {
	Logger::logStream(Logger::Warning) << "Warning: Could not find barony "
					   << (*holding)->getKey()
					   << " in province "
					   << nameAndNumber(ckProvince)
					   << ".\n";
	continue;
      }
      
      Object* bishop = getChar(ckTitle->safeGetString("holder"));
      if (!bishop) continue;
      if (bishop->safeGetString("religion") != "\"catholic\"") continue;
      if (find(bishops.begin(), bishops.end(), bishop) != bishops.end()) continue; 
      if (0 == ckProvToEuProvsMap[ckProvince].size()) continue;
    
      calculateAttributes(bishop);
      bishops.push_back(bishop);
      bishop->resetLeaf("bishopric", ckTitle->getKey()); 
      Object* euProv = ckProvToEuProvsMap[ckProvince][0];
      euProvs.push_back(euProv);
    }
  }

  ObjectAscendingSorter sorter("learning");
  sort(bishops.begin(), bishops.end(), sorter);
  objvec cardinals = papacy->getValue("cardinal");
  for (objiter c = cardinals.begin(); c != cardinals.end(); ++c) {
    if (0 == bishops.size()) {
      Logger::logStream(DebugReligion) << "Removed cardinal "
				       << (*c)->safeGetString("name")
				       << " due to lack of bishops.\n"; 
      papacy->removeObject(*c);
      continue;
    }

    Object* bishop = bishops.back();
    Object* euProv = euProvs.back(); 
    bishops.pop_back();
    euProvs.pop_back();

    Logger::logStream(DebugReligion) << "Cardinal " << (*c)->safeGetString("name");
      
    (*c)->resetLeaf("name", bishop->safeGetString("birth_name"));
    (*c)->resetLeaf("location", euProv->getKey());
    (*c)->resetLeaf("controller", euProv->safeGetString("owner"));
    Logger::logStream(DebugReligion) << " assigned to " << euProv->safeGetString("owner")
				     << " due to bishop " << bishop->safeGetString("birth_name")
				     << " of " << bishop->safeGetString("bishopric")
				     << ", learning " << bishop->safeGetString("learning")
				     << ".\n"; 
    
  }
}

void WorkerThread::eu3ProvinceCultures () {
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    objvec ckps = (*link).second;
    if (0 == ckps.size()) continue; // Already gave a warning about this.

    map<string, double> cultureWeights; 
    
    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
      string ckCulture = (*ckp)->safeGetString("culture");
      if (cultureMap.find(ckCulture) == cultureMap.end()) {
	Logger::logStream(Logger::Warning) << "Warning: Could not find EU3 conversion for culture "
					   << ckCulture
					   << " in province "
					   << nameAndNumber(*ckp)
					   << ", no weight assigned.\n";
	continue; 
      }

      string euCulture = cultureMap[ckCulture];      
      string countyTitle = remQuotes((*ckp)->safeGetString("title"));
      Object* ckTitle = titleMap[countyTitle];
      while (ckTitle) {
	string dejure = remQuotes(ckTitle->safeGetString("de_jure_liege", "---"));
	// If it is currently under its dejure liege, it's not listed, so check for actual liege as backup. 
	if (dejure == "---") dejure = remQuotes(ckTitle->safeGetString("liege", "---"));
	if (dejure == "---") break; 
	if (specialCultureMap.find(dejure) != specialCultureMap.end()) {
	  string candidate = specialCultureMap[dejure][ckCulture];
	  if (candidate != "") euCulture = candidate;
	  break; 
	}
	ckTitle = titleMap[dejure]; 
      }
      cultureWeights[euCulture] += getCkWeight(*ckp, BaseTax); 
    }

    if (0 == cultureWeights.size()) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find any conversions for CK cultures in EU province "
					 << nameAndNumber(eup)
					 << ", no change made.\n";
      continue; 
    }
    
    double best = 0;
    string winner = eup->safeGetString("culture");
    for (map<string, double>::iterator cand = cultureWeights.begin(); cand != cultureWeights.end(); ++cand) {
      if ((*cand).second < best) continue;
      winner = (*cand).first;
      best = (*cand).second;
    }
    if (winner == eup->safeGetString("culture")) continue;

    Logger::logStream(DebugCulture) << "Changing culture of "
				    << nameAndNumber(eup)
				    << " to "
				    << winner
				    << " from "
				    << eup->safeGetString("culture") 
				    << ".\n";
    Object* history = eup->getNeededObject("history");    
    eup->resetLeaf("culture", winner);
    history->resetLeaf("culture", winner);
  }
}

void WorkerThread::eu3ProvinceReligion () {
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    objvec ckps = (*link).second;
    if (0 == ckps.size()) continue; // Already gave a warning about this.

    map<string, double> weights; 
    
    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
      string ckReligion = (*ckp)->safeGetString("religion");
      if (religionMap.find(ckReligion) == religionMap.end()) {
	Logger::logStream(Logger::Warning) << "Warning: Could not find EU3 conversion for religion "
					   << ckReligion
					   << " in province "
					   << nameAndNumber(*ckp)
					   << ", no weight assigned.\n";
	continue; 
      }

      string euReligion = religionMap[ckReligion];      
      weights[euReligion] += getCkWeight(*ckp, BaseTax); 
    }

    if (0 == weights.size()) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find any conversions for CK religions in EU province "
					 << nameAndNumber(eup)
					 << ", no change made.\n";
      continue; 
    }
    
    double best = 0;
    string winner = eup->safeGetString("religion");
    for (map<string, double>::iterator cand = weights.begin(); cand != weights.end(); ++cand) {
      if ((*cand).second < best) continue;
      winner = (*cand).first;
      best = (*cand).second;
    }
    if (winner == eup->safeGetString("religion")) continue;

    Logger::logStream(DebugReligion) << "Changing religion of "
				    << nameAndNumber(eup)
				    << " to "
				    << winner
				    << " from "
				    << eup->safeGetString("religion") 
				    << ".\n";
    Object* history = eup->getNeededObject("history");        
    eup->resetLeaf("religion", winner);
    history->resetLeaf("religion", winner);     
  }
}

void WorkerThread::eu3Provinces () {
  double maxCkFortLevel = 0; 
  
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    Object* history = eup->getNeededObject("history"); 
    eup->unsetValue("hre"); history->unsetValue("hre");
    eup->unsetValue("temple"); history->unsetValue("temple");
    eup->unsetValue("workshop"); history->unsetValue("workshop");
    eup->unsetValue("marketplace"); history->unsetValue("marketplace");
    eup->unsetValue("dock"); history->unsetValue("dock");
    eup->unsetValue("armory"); history->unsetValue("armory");
    eup->unsetValue("courthouse"); history->unsetValue("courthouse");
    eup->unsetValue("customs_house"); history->unsetValue("customs_house");
    eup->unsetValue("shipyard"); history->unsetValue("shipyard");
    eup->unsetValue("regimental_camp"); history->unsetValue("regimental_camp");
    eup->unsetValue("constable"); history->unsetValue("constable");
    eup->unsetValue("tax_assessor"); history->unsetValue("tax_assessor");
    eup->unsetValue("war_college"); history->unsetValue("war_college");
    eup->unsetValue("admiralty"); history->unsetValue("admiralty");
    eup->unsetValue("refinery"); history->unsetValue("refinery");
    eup->unsetValue("wharf"); history->unsetValue("wharf");
    eup->unsetValue("university"); history->unsetValue("university");
    eup->unsetValue("fine_arts_academy"); history->unsetValue("fine_arts_academy");
    eup->unsetValue("textile");     history->unsetValue("textile");    
    eup->unsetValue("fort1"); history->unsetValue("fort1");
    eup->unsetValue("fort2"); history->unsetValue("fort2");
    eup->unsetValue("fort3"); history->unsetValue("fort3");
    eup->unsetValue("fort4"); history->unsetValue("fort4");
    eup->unsetValue("fort5"); history->unsetValue("fort5");
    eup->unsetValue("fort6"); history->unsetValue("fort6");

    objvec ckps = (*link).second;
    if (0 == ckps.size()) {
      Logger::logStream(Logger::Warning) << "Warning: No CK provinces for "
					 << eup->getKey()
					 << ", no reassignment made.\n";
      continue; 
    }

    map<string, int> discMap;
    Object* disc = eup->getNeededObject("discovered_by");
    disc->setObjList(); 
    for (int i = 0; i < disc->numTokens(); ++i) discMap[disc->getToken(i)]++;
    for (map<Object*, Object*>::iterator i = euCountryToCkCountryMap.begin(); i != euCountryToCkCountryMap.end(); ++i) {
      string tag = (*i).first->getKey(); 
      if (0 < discMap[tag]) continue;
      disc->addToList(tag); 
    }
    
    map<Object*, double> own_weights;
    map<Object*, double> con_weights;
    double owner_con_weight = 0;    
    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
      maxCkFortLevel = max(maxCkFortLevel, getCkWeight(*ckp, FortLevel)); 
      
      objvec leaves = (*ckp)->getLeaves();
      for (objiter leaf = leaves.begin(); leaf != leaves.end(); ++leaf) {
	if ((*leaf)->safeGetString("type", "BLAH") == "BLAH") continue; // Not a holding
	string controllerId = (*leaf)->safeGetString("controller", "BLAH");
	if (controllerId == "BLAH") owner_con_weight++;
	else {
	  Object* controller = getChar(controllerId);
	  if ((controller) && (characterToEuCountryMap[controller])) con_weights[characterToEuCountryMap[controller]] += getCkWeight(*ckp, BaseTax);
	  else owner_con_weight += getCkWeight(*ckp, BaseTax); 
	}
      }
      
      string ownerId = (*ckp)->safeGetString("owner");
      Object* owner = getChar(ownerId);
      if (!owner) {
	Logger::logStream(Logger::Warning) << "Warning: Could not find owner "
					   << ownerId
					   << " of province "
					   << (*ckp)->getKey()
					   << ".\n";
	continue;
      }

      Object* cand = characterToEuCountryMap[owner];
      if (!cand) {
	Logger::logStream(Logger::Warning) << "Warning: Unable to find owner for CK province "
					   << (*ckp)->getKey()
					   << ".\n";
	continue; 
      }
      own_weights[cand] += getCkWeight(*ckp, BaseTax);
    }

    Object* winner = 0;
    double best = owner_con_weight;
    string conTag = ""; 
    for (map<Object*, double>::iterator curr = own_weights.begin(); curr != own_weights.end(); ++curr) {
      if ((*curr).second <= best) continue;
      winner = (*curr).first;
      best = (*curr).second; 
    }
    if (winner) conTag = addQuotes(winner->getKey());
    
    if (own_weights.empty()) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find any owners for EU province "
					 << eup->getKey()
					 << ", no reassignment made.\n";
      continue; 
    }
    
    winner = 0;
    best = 0;
    for (map<Object*, double>::iterator curr = own_weights.begin(); curr != own_weights.end(); ++curr) {
      if ((*curr).second <= best) continue;
      winner = (*curr).first;
      best = (*curr).second;

    }

    eup->resetLeaf("owner", addQuotes(winner->getKey()));
    eup->resetLeaf("controller", conTag == "" ? addQuotes(winner->getKey()) : conTag);
    history->resetLeaf("owner", addQuotes(winner->getKey()));
    history->resetLeaf("controller", conTag == "" ? addQuotes(winner->getKey()) : conTag);

    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
      if (find(euCountryToCkProvincesMap[winner].begin(), euCountryToCkProvincesMap[winner].end(), (*ckp)) != euCountryToCkProvincesMap[winner].end()) continue; 
      euCountryToCkProvincesMap[winner].push_back(*ckp);
    }
  }

  // Rebuild forts
  Object* fortLevels = configObject->getNeededObject("fortLevels");
  objvec forts = fortLevels->getLeaves(); 
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    Object* history = eup->getNeededObject("history"); 

    objvec ckps = (*link).second;
    if (0 == ckps.size()) continue;

    string keyword = "NONE";
    double passed = 0;
    for (objiter level = forts.begin(); level != forts.end(); ++level) {
      string currKey = (*level)->getKey();
      double current = fortLevels->safeGetFloat(currKey);
      for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
	double currentFortWeight = getCkWeight(*ckp, FortLevel);
	if (currentFortWeight < maxCkFortLevel * current) continue; // Not enough fort level for this.
	if (current < passed) continue; // We already did better.
	keyword = currKey;
	passed = current; 
      }
    }
    if (keyword == "NONE") continue;
    eup->resetLeaf(keyword, "yes");
    history->resetLeaf(keyword, "yes");
  }
  
  
}

struct SliderInfo {
  double minimumCKvalue;
  double maximumCKvalue;
  int minimumEUvalue;
  int maximumEUvalue; 
}; 

void WorkerThread::eu3Sliders () {
  Object* sliders = configObject->safeGetObject("sliders");
  if (!sliders) {
    Logger::logStream(Logger::Warning) << "Could not find slider object in config.txt. No changes made to sliders.\n";
    return;
  }
  Object* techs = configObject->safeGetObject("tech_slider_effects");
  objvec techEffects = techs->getLeaves(); 

  Object* builds = configObject->safeGetObject("building_slider_effects");
  objvec buildEffects = builds->getLeaves(); 
  
  map<string, SliderInfo*> values;
  objvec names = sliders->getLeaves();
  for (objiter slider = names.begin(); slider != names.end(); ++slider) {
    SliderInfo* curr = new SliderInfo();
    string key = (*slider)->getKey();
    curr->minimumEUvalue = (*slider)->tokenAsInt(0);
    curr->maximumEUvalue = (*slider)->tokenAsInt(1);
    curr->minimumCKvalue = 1e20;
    curr->maximumCKvalue = -1e20; 
    values[key] = curr; 
  }
  
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    if ((euCountry->getKey() == "PIR") || (euCountry->getKey() == "REB")) continue;
    Object* ckRuler = (*i).second;

    map<string, double> currentCKvals;     
    for (objiter title = beginRel(ckRuler, Title); title != finalRel(ckRuler, Title); ++title) {
      objvec laws = (*title)->getValue("law");
      for (objiter law = laws.begin(); law != laws.end(); ++law) {
	string key = remQuotes((*law)->getLeaf());
	Object* effect = configObject->safeGetObject(key);
	if (!effect) continue;
	for (map<string, SliderInfo*>::iterator name = values.begin(); name != values.end(); ++name) {
	  string key = (*name).first;
	  currentCKvals[key] += effect->safeGetFloat(key) * (1 + titleTier(*title));
	}
      }
    }

    for (objiter ckp = euCountryToCkProvincesMap[euCountry].begin(); ckp != euCountryToCkProvincesMap[euCountry].end(); ++ckp) {
      Object* province = (*ckp);
      
      objvec baronies = province->getLeaves();
      for (objiter barony = baronies.begin(); barony != baronies.end(); ++barony) {
      	string type = (*barony)->safeGetString("type", "BLAH");
	if (type == "BLAH") continue;

	Object* buildList = ckBuildings->safeGetObject(type);
	if (buildList) { 
	  objvec buildings = buildList->getLeaves();
	  for (objiter building = buildings.begin(); building != buildings.end(); ++building) {
	    if ((*barony)->safeGetString((*building)->getKey(), "no") != "yes") continue;
	    
	    for (objiter beff = buildEffects.begin(); beff != buildEffects.end(); ++beff) {
	      double number = (*building)->safeGetFloat((*beff)->getKey()); 
	      for (map<string, SliderInfo*>::iterator name = values.begin(); name != values.end(); ++name) {
		string key = (*name).first;
		currentCKvals[key] += (*beff)->safeGetFloat(key) * number; 
	      }		      
	    }
	  }
	}

	
	Object* effect = configObject->safeGetObject(type);
	if (!effect) continue;
	for (map<string, SliderInfo*>::iterator name = values.begin(); name != values.end(); ++name) {
	  string key = (*name).first;
	  currentCKvals[key] += effect->safeGetFloat(key); 
	}	
      }

      Object* provTechs = province->safeGetObject("technology");
      if (provTechs) provTechs = provTechs->safeGetObject("level");
      if (!provTechs) continue;

      for (objiter effect = techEffects.begin(); effect != techEffects.end(); ++effect) {
	string techstring = (*effect)->getKey();
	int techNumber = atoi(techstring.c_str());
	for (map<string, SliderInfo*>::iterator name = values.begin(); name != values.end(); ++name) {
	  string key = (*name).first;
	  currentCKvals[key] += (*effect)->safeGetFloat(key) * provTechs->tokenAsInt(techNumber);
	}	
      }
    }

    for (map<string, SliderInfo*>::iterator name = values.begin(); name != values.end(); ++name) {
      string key = (*name).first;
      ckRuler->resetLeaf(key, currentCKvals[key]);
      if ((*name).second->maximumCKvalue < currentCKvals[key]) (*name).second->maximumCKvalue = currentCKvals[key];
      if ((*name).second->minimumCKvalue > currentCKvals[key]) (*name).second->minimumCKvalue = currentCKvals[key];
    }	
  }

  for (map<string, SliderInfo*>::iterator name = values.begin(); name != values.end(); ++name) {
    (*name).second->maximumEUvalue -= (*name).second->minimumEUvalue;
    if (fabs((*name).second->maximumCKvalue - (*name).second->minimumCKvalue) < 0.001)
      Logger::logStream(Logger::Warning) << "Warning: No apparent information on slider "
					 << (*name).first
					 << ", skipping without change for all nations.\n"; 
  }
  
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    if ((euCountry->getKey() == "PIR") || (euCountry->getKey() == "REB")) continue;
    Object* ckRuler = (*i).second;

    for (map<string, SliderInfo*>::iterator name = values.begin(); name != values.end(); ++name) {
      string key = (*name).first;
      double currValue = ckRuler->safeGetFloat(key);
      double currEUmin = (*name).second->minimumEUvalue;
      double currEUmax = (*name).second->maximumEUvalue;
      double currCKmin = (*name).second->minimumCKvalue;
      double currCKmax = (*name).second->maximumCKvalue;

      if (fabs(currCKmax - currCKmin) < 0.001) continue; 

      currCKmax -= currCKmin; // Maximums are actually differences
      currValue -= currCKmin;
      currValue /= currCKmax;
      currValue *= currEUmax;
      currValue += currEUmin;
      int final = (int) floor(currValue + 0.5);

      euCountry->resetLeaf(key, final);
      Object* history = euCountry->getNeededObject("history");
      history->resetLeaf(key, final);
      
      Logger::logStream(DebugSliders) << "Setting " << euCountry->getKey() << " " << key << " to " << final
				      << " based on score " << ckRuler->safeGetString(key) << " of "
				      << currEUmin << " " << currEUmax << " " << currCKmin << " " << currCKmax << " " << currValue 
				      << ".\n"; 
      
    }
  }
}

void WorkerThread::eu3StateCultures () {
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    if ((euCountry->getKey() == "PIR") || (euCountry->getKey() == "REB")) continue; 
    
    Object* ckRuler   = (*i).second;
    map<string, double> weights; 
    
    recursiveCollectCultures(ckRuler, weights, 1);

    if (0 == weights.size()) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find any EU3 cultures for tag "
					 << euCountry->getKey()
					 << ", no change made.\n";
      continue;
    }
    
    map<string, double>::iterator best = weights.begin();
    for (map<string, double>::iterator cand = weights.begin(); cand != weights.end(); ++cand) {
      if ((*cand).second < (*best).second) continue;
      best = cand;
    }

    Object* history = euCountry->getNeededObject("history");
    string oldCulture = euCountry->safeGetString("primary_culture");
    if (oldCulture != (*best).first) {
      Logger::logStream(DebugCulture) << "New primary culture for "
				      << euCountry->getKey() << ": "
				      << (*best).first << " (was " << oldCulture
				      << ")\n";
      euCountry->resetLeaf("primary_culture", (*best).first);
      history->resetLeaf("primary_culture", (*best).first);
      
    }

    euCountry->unsetValue("accepted_culture");
    history->unsetValue("accepted_culture");
    
    for (map<string, double>::iterator cand = weights.begin(); cand != weights.end(); ++cand) {
      if (best == cand) continue;
      if ((*cand).second < 0.20*(*best).second) continue;
      euCountry->setLeaf("accepted_culture", (*cand).first);
      history->setLeaf("add_accepted_culture", (*cand).first);

      Logger::logStream(DebugCulture) << "Adding " << (*cand).first
				      << " as accepted culture of "
				      << euCountry->getKey()
				      << ".\n"; 
    }
  }
}

void WorkerThread::eu3StateReligion () {
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    if ((euCountry->getKey() == "PIR") || (euCountry->getKey() == "REB")) continue; 
    
    Object* ckRuler   = (*i).second;
    map<string, double> weights; 
    
    recursiveCollectReligion(ckRuler, weights, 1);

    if (0 == weights.size()) {
      Logger::logStream(Logger::Warning) << "Warning: Could not find any EU3 religion for tag "
					 << euCountry->getKey()
					 << ", no change made.\n";
      continue;
    }
    
    map<string, double>::iterator best = weights.begin();
    for (map<string, double>::iterator cand = weights.begin(); cand != weights.end(); ++cand) {
      if ((*cand).second < (*best).second) continue;
      best = cand;
    }

    Object* history = euCountry->getNeededObject("history");
    string oldReligion = euCountry->safeGetString("religion");
    if (oldReligion != (*best).first) {
      Logger::logStream(DebugReligion) << "New state religion for "
				      << euCountry->getKey() << ": "
				      << (*best).first << " (was " << oldReligion
				      << ")\n";
      euCountry->resetLeaf("religion", (*best).first);
      history->resetLeaf("religion", (*best).first);
      
    }
  }
}


void WorkerThread::eu3StateVariables () {
  double maxEuPrestige = configObject->safeGetFloat("minimumMaxPrestige", 20);
  double maxCkPrestige = 0; 
  double totalEuGold   = 0;
  double totalCkGold   = 0; 
  
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    Object* ckRuler   = (*i).second;
    
    totalEuGold += euCountry->safeGetFloat("treasury");
    totalCkGold += max(ckRuler->safeGetFloat("wealth"), 0.0); 
    maxEuPrestige = max(maxEuPrestige, fabs(euCountry->safeGetFloat("precise_prestige")));
    maxCkPrestige = max(maxCkPrestige, fabs(ckRuler->safeGetFloat("prestige") + ckRuler->safeGetFloat("piety")));

    euCountry->resetLeaf("stability", "0.000");
    euCountry->resetLeaf("stability_investment", "0.000");
    euCountry->resetLeaf("badboy", "0.000"); 
    euCountry->resetLeaf("infantry", "\"western_medieval_infantry\"");
    euCountry->resetLeaf("cavalry", "\"western_medieval_knights\"");
    euCountry->resetLeaf("manpower", "1.000");
    euCountry->resetLeaf("big_ship", "\"carrack\"");
    euCountry->resetLeaf("galley", "\"galley\"");
    euCountry->resetLeaf("transport", "\"cog\"");
    euCountry->resetLeaf("colonists", "0");
    euCountry->resetLeaf("missionaries", "0");
    euCountry->resetLeaf("merchants", "0");
    euCountry->resetLeaf("spies", "0");
    euCountry->resetLeaf("diplomats", "0");
    euCountry->resetLeaf("officials", "0");
    euCountry->resetLeaf("army_tradition", "0");    
    euCountry->resetLeaf("cultural_tradition", "0");
    euCountry->resetLeaf("navy_tradition", "0");
    euCountry->resetLeaf("inflation", "0.000");
    euCountry->resetLeaf("unit_type", "western"); 
    euCountry->resetLeaf("war_exhaustion", "0.000");    
    euCountry->resetLeaf("legitimacy", "1.000");
    euCountry->resetLeaf("technology_group", "western"); 
    euCountry->unsetValue("leader");

    Object* history = euCountry->getNeededObject("history");
    euCountry->unsetValue("advisor"); history->unsetValue("advisor"); 
    euCountry->unsetValue("press_gangs"); history->unsetValue("press_gangs");
    euCountry->unsetValue("grand_navy"); history->unsetValue("grand_navy");
    euCountry->unsetValue("sea_hawks"); history->unsetValue("sea_hawks");
    euCountry->unsetValue("superior_seamanship"); history->unsetValue("superior_seamanship");
    euCountry->unsetValue("naval_glory"); history->unsetValue("naval_glory");
    euCountry->unsetValue("excellent_shipwrights"); history->unsetValue("excellent_shipwrights");
    euCountry->unsetValue("naval_fighting_instruction"); history->unsetValue("naval_fighting_instruction");
    euCountry->unsetValue("naval_provisioning"); history->unsetValue("naval_provisioning");
    euCountry->unsetValue("grand_army"); history->unsetValue("grand_army");
    euCountry->unsetValue("military_drill"); history->unsetValue("military_drill");
    euCountry->unsetValue("engineer_corps"); history->unsetValue("engineer_corps");
    euCountry->unsetValue("battlefield_commisions"); history->unsetValue("battlefield_commisions");
    euCountry->unsetValue("glorious_arms"); history->unsetValue("glorious_arms");
    euCountry->unsetValue("national_conscripts"); history->unsetValue("national_conscripts");
    euCountry->unsetValue("regimental_system"); history->unsetValue("regimental_system");
    euCountry->unsetValue("napoleonic_warfare"); history->unsetValue("napoleonic_warfare");
    euCountry->unsetValue("land_of_opportunity"); history->unsetValue("land_of_opportunity");
    euCountry->unsetValue("merchant_adventures"); history->unsetValue("merchant_adventures");
    euCountry->unsetValue("colonial_ventures"); history->unsetValue("colonial_ventures");
    euCountry->unsetValue("shrewd_commerce_practise"); history->unsetValue("shrewd_commerce_practise");
    euCountry->unsetValue("vice_roys"); history->unsetValue("vice_roys");
    euCountry->unsetValue("quest_for_the_new_world"); history->unsetValue("quest_for_the_new_world");
    euCountry->unsetValue("scientific_revolution"); history->unsetValue("scientific_revolution");
    euCountry->unsetValue("improved_foraging"); history->unsetValue("improved_foraging");
    euCountry->unsetValue("vetting"); history->unsetValue("vetting");
    euCountry->unsetValue("bureaucracy"); history->unsetValue("bureaucracy");
    euCountry->unsetValue("national_bank"); history->unsetValue("national_bank");
    euCountry->unsetValue("national_trade_policy"); history->unsetValue("national_trade_policy");
    euCountry->unsetValue("espionage"); history->unsetValue("espionage");
    euCountry->unsetValue("bill_of_rights"); history->unsetValue("bill_of_rights");
    euCountry->unsetValue("smithian_economics"); history->unsetValue("smithian_economics");
    euCountry->unsetValue("liberty_egalite_fraternity"); history->unsetValue("liberty_egalite_fraternity");
    euCountry->unsetValue("ecumenism"); history->unsetValue("ecumenism");
    euCountry->unsetValue("church_attendance_duty"); history->unsetValue("church_attendance_duty");
    euCountry->unsetValue("divine_supremacy"); history->unsetValue("divine_supremacy");
    euCountry->unsetValue("patron_of_art"); history->unsetValue("patron_of_art");
    euCountry->unsetValue("deus_vult"); history->unsetValue("deus_vult");
    euCountry->unsetValue("humanist_tolerance"); history->unsetValue("humanist_tolerance");
    euCountry->unsetValue("cabinet"); history->unsetValue("cabinet");
    euCountry->unsetValue("revolution_and_counter"); history->unsetValue("revolution_and_counter");



    
  }

  double minimumGold = configObject->safeGetFloat("minimumGold", 10);  
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    Object* ckRuler   = (*i).second;

    double gold = max(ckRuler->safeGetFloat("wealth"), 0.0); 
    double prestige = ckRuler->safeGetFloat("prestige") + ckRuler->safeGetFloat("piety");

    Logger::logStream(DebugGovernments) << "Prestige for " << euCountry->getKey() << ": "
					<< prestige << " of " << maxCkPrestige << " and " << maxEuPrestige
					<< " gives " << (prestige * maxEuPrestige / maxCkPrestige) << ".\n"; 
    
    gold /= totalCkGold;
    gold *= totalEuGold;
    gold = max(gold, minimumGold); 
    euCountry->resetLeaf("treasury", gold);

    prestige /= maxCkPrestige;
    prestige *= maxEuPrestige;
    euCountry->resetLeaf("precise_prestige", prestige);

    Object* history = euCountry->getNeededObject("history");
    Object* demesne = ckRuler->safeGetObject("demesne");
    if (!demesne) continue;
    string capTag = remQuotes(demesne->safeGetString("capital", "BLAH"));
    Object* capTitle = getTitle(capTag);
    if (!capTitle) {
      Logger::logStream(Logger::Warning) << "Warning: Unable to find CK capital " << capTag
					 << " for tag " << euCountry->getKey() << "; not resetting capital.\n";
      continue; 
    }

    Object* capProv = titleToCkProvinceMap[capTitle];
    if (!capProv) {
      Logger::logStream(Logger::Warning) << "Warning: Unable to find CK province for capital title " << capTitle->getKey()
					 << " for tag " << euCountry->getKey() << "; not resetting capital.\n";
      continue; 
    }

    Object* newCapital = 0;
    for (objiter cand = ckProvToEuProvsMap[capProv].begin(); cand != ckProvToEuProvsMap[capProv].end(); ++cand) {
      string ownerTag = remQuotes((*cand)->safeGetString("owner"));
      if (ownerTag != euCountry->getKey()) continue;
      newCapital = (*cand);
      break; 
    }

    if (!newCapital) {
      for (objiter t = beginRel(ckRuler, Title); t != finalRel(ckRuler, Title); ++t) {
	Object* ckProv = titleToCkProvinceMap[*t];
	if (!ckProv) continue;
	for (objiter cand = ckProvToEuProvsMap[ckProv].begin(); cand != ckProvToEuProvsMap[ckProv].end(); ++cand) {
	  string ownerTag = remQuotes((*cand)->safeGetString("owner"));
	  if (ownerTag != euCountry->getKey()) continue;
	  newCapital = (*cand);
	  break; 
	}
	if (newCapital) break; 
      }
    }

    if (!newCapital) {
      // Bah. Search every EU3 province until we find one!
      for (map<Object*, objvec>::iterator i = euProvToCkProvsMap.begin(); i != euProvToCkProvsMap.end(); ++i) {
	Object* euProv = (*i).first;
	if (remQuotes(euProv->safeGetString("owner")) != euCountry->getKey()) continue;
	newCapital = euProv;
	break; 
      }
    }
    
    if (!newCapital) {
      Logger::logStream(Logger::Warning) << "Could not find capital province for tag " << euCountry->getKey()
					 << ", no reassignment made.\n";
      continue; 
    }

    Logger::logStream(DebugGovernments) << "Moving " << euCountry->getKey() << " capital to " << newCapital->getKey() << ".\n"; 
    euCountry->resetLeaf("capital", newCapital->getKey());
    history->resetLeaf("capital", newCapital->getKey());
  }  
}


void WorkerThread::eu3Taxes () {
  double totalEuTax = 0;
  double totalCkWeight = 0; 
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    totalEuTax += (*link).first->safeGetFloat("base_tax");
    for (objiter ckp = (*link).second.begin(); ckp != (*link).second.end(); ++ckp) {
      totalCkWeight += getCkWeight(*ckp);
    }    
  }

  Logger::logStream(DebugBasetax) << "Total taxes before redistribution: " << totalEuTax << "\n";
  
  double overflow = 0;
  double afterTotal = 0;
  map<string, pair<double, double> > gains; 
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first; 
    
    double currWeight = 0; 
    for (objiter ckp = (*link).second.begin(); ckp != (*link).second.end(); ++ckp) {
      currWeight += getCkWeight(*ckp);
    }
    currWeight /= totalCkWeight;
    currWeight *= totalEuTax;

    currWeight += overflow;
    double basetax = floor(currWeight + 0.5);
    overflow = (currWeight - basetax);

    double inputValue = eup->safeGetFloat("base_tax");
    Logger::logStream(DebugBasetax) << "Province " << nameAndNumber(eup) << " has initial tax " << inputValue
				    << ", final tax " << basetax << "\n";
    gains[eup->safeGetString("owner")].first  += inputValue;
    gains[eup->safeGetString("owner")].second += basetax;     
    
    afterTotal += basetax; 
    eup->resetLeaf("base_tax", basetax);
    Object* history = eup->getNeededObject("history");
    history->resetLeaf("base_tax", basetax); 
  }

  Logger::logStream(DebugBasetax) << "After distribution: " << afterTotal << "\n"
				  << "Gains and losses (not showing changes of 9 or less):\n"
				  << "Tag\tInitial\tFinal\tChange\n"; 
  for (map<string, pair<double, double> >::iterator gain = gains.begin(); gain != gains.end(); ++gain) {
    double change = (*gain).second.second - (*gain).second.first; 
    if (fabs(change) < 9.1) continue; 
    Logger::logStream(DebugBasetax) << remQuotes((*gain).first) << "\t"
				    << (*gain).second.first << "\t"
				    << (*gain).second.second << "\t"
				    << change 
				    << "\n"; 
  }
}

void WorkerThread::eu3Techs () {
  map<Object*, objvec> ownerMap; 
  
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    Object* euCountry = euxGame->safeGetObject(remQuotes(eup->safeGetString("owner")));
    if (!euCountry) continue;
    ownerMap[euCountry].push_back(eup); 
      
    for (objiter ckp = (*link).second.begin(); ckp != (*link).second.end(); ++ckp) {
      eup->resetLeaf("navyWeight", eup->safeGetFloat("navyWeight") + getCkWeight(*ckp, Galleys));
      eup->resetLeaf("armyWeight", eup->safeGetFloat("armyWeight") + getCkWeight(*ckp, ManPower));
      eup->resetLeaf("govtWeight", eup->safeGetFloat("govtWeight") + getCkWeight(*ckp, BaseTax));      
    }
  }

  double maxNavy = 1;
  double maxArmy = 1;
  double maxGovt = 1;
  ObjectAscendingSorter nsorter("navyWeight");
  ObjectAscendingSorter asorter("armyWeight");
  ObjectAscendingSorter gsorter("govtWeight");  
  for (map<Object*, objvec>::iterator eun = ownerMap.begin(); eun != ownerMap.end(); ++eun) {
    unsigned int numProvs = (*eun).second.size();
    if (0 == numProvs) continue;    
    sort((*eun).second.begin(), (*eun).second.end(), nsorter);
    double currNavy = (*eun).second[numProvs / 2]->safeGetFloat("navyWeight");
    if (0 == numProvs%2) {
      currNavy += (*eun).second[(numProvs / 2) - 1]->safeGetFloat("navyWeight");
      currNavy *= 0.5; 
    }
    double currArmy = (*eun).second[numProvs / 2]->safeGetFloat("armyWeight");
    if (0 == numProvs%2) {
      currArmy += (*eun).second[(numProvs / 2) - 1]->safeGetFloat("armyWeight");
      currArmy *= 0.5; 
    }
    double currGovt = (*eun).second[numProvs / 2]->safeGetFloat("govtWeight");
    if (0 == numProvs%2) {
      currGovt += (*eun).second[(numProvs / 2) - 1]->safeGetFloat("govtWeight");
      currGovt *= 0.5; 
    }
    
    maxNavy = max(maxNavy, currNavy);
    maxArmy = max(maxArmy, currArmy);
    maxGovt = max(maxGovt, currGovt);    

    (*eun).first->resetLeaf("navyWeight", currNavy);
    (*eun).first->resetLeaf("armyWeight", currArmy);
    (*eun).first->resetLeaf("govtWeight", currGovt);
  }


  Object* tl = configObject->getNeededObject("techLevels");
  objvec techLevels = tl->getLeaves(); 
  for (map<Object*, objvec>::iterator eun = ownerMap.begin(); eun != ownerMap.end(); ++eun) {
    double pNavy = -1;
    double pArmy = -1;
    double pGovt = -1;    

    Object* euCountry = (*eun).first;
    Object* techs = euCountry->getNeededObject("technology");
    Object* history = euCountry->getNeededObject("history");
    Object* firstDate = history->getNeededObject(remQuotes(euxGame->safeGetString("start_date"))); 
    for (objiter level = techLevels.begin(); level != techLevels.end(); ++level) {
      string levelToSet = (*level)->getKey();
      double neededToPass = tl->safeGetFloat(levelToSet);

      if ((neededToPass > pNavy) && (neededToPass < euCountry->safeGetFloat("navyWeight") / maxNavy)) {
	Object* navyTech = techs->getNeededObject("naval_tech");
	navyTech->clear();
	navyTech->setObjList();
	navyTech->addToList(levelToSet);
	navyTech->addToList("0.000");
	firstDate->resetLeaf("naval_tech", levelToSet);
	pNavy = neededToPass;
      }
      if ((neededToPass > pArmy) && (neededToPass < euCountry->safeGetFloat("armyWeight") / maxArmy)) {
	Object* armyTech = techs->getNeededObject("land_tech");
	armyTech->clear();
	armyTech->setObjList();
	armyTech->addToList(levelToSet);
	armyTech->addToList("0.000");
	firstDate->resetLeaf("land_tech", levelToSet);
	pArmy = neededToPass;
      }
      if ((neededToPass > pGovt) && (neededToPass < euCountry->safeGetFloat("govtWeight") / maxGovt)) {
	Object* govtTech = techs->getNeededObject("trade_tech");
	govtTech->clear();
	govtTech->setObjList();
	govtTech->addToList(levelToSet);
	govtTech->addToList("0.000");
	govtTech = techs->getNeededObject("production_tech");
	govtTech->clear();
	govtTech->setObjList();
	govtTech->addToList(levelToSet);
	govtTech->addToList("0.000");
	govtTech = techs->getNeededObject("government_tech");
	govtTech->clear();
	govtTech->setObjList();
	govtTech->addToList(levelToSet);
	govtTech->addToList("0.000");

	firstDate->resetLeaf("trade_tech", levelToSet);
	firstDate->resetLeaf("production_tech", levelToSet);
	firstDate->resetLeaf("government_tech", levelToSet);
	pGovt = neededToPass;
      }

      
    }
    
  }


  for (map<Object*, objvec>::iterator eun = ownerMap.begin(); eun != ownerMap.end(); ++eun) {
    Object* euCountry = (*eun).first;
    Object* techs = euCountry->getNeededObject("technology");
    Logger::logStream(DebugTechTeams) << "Tag " << euCountry->getKey() << " has weights "
				      << euCountry->safeGetString("navyWeight") << " "
				      << euCountry->safeGetString("armyWeight") << " "
				      << euCountry->safeGetString("govtWeight") << " giving techs "
				      << techs->safeGetObject("naval_tech")->getToken(0) << " "
				      << techs->safeGetObject("land_tech")->getToken(0) << " "
				      << techs->safeGetObject("trade_tech")->getToken(0) << ".\n"; 
      

    
    euCountry->unsetValue("navyWeight");
    euCountry->unsetValue("armyWeight");
    euCountry->unsetValue("govtWeight");    
    for (objiter eup = (*eun).second.begin(); eup != (*eun).second.end(); ++eup) {
      (*eup)->unsetValue("navyWeight");
      (*eup)->unsetValue("armyWeight");
      (*eup)->unsetValue("govtWeight");      
    }
  }
}

/******************************* End EU3 conversions ********************************/

/*******************************  Begin calculators ********************************/

void WorkerThread::calculateAttributes (Object* ckChar) {
  if (!ckChar) return;
  if (-999 != ckChar->safeGetInt("martial", -999)) return; 
  
  Object* attributes = ckChar->safeGetObject("attributes");
  if (attributes) {
    for (int i = 0; i < 5; ++i) ckChar->resetLeaf(attribNames[i], attributes->tokenAsInt(i));
  }

  Object* ckTraits = ckChar->safeGetObject("traits");
  if (!ckTraits) return;
  for (int t = 0; t < ckTraits->numTokens(); ++t) {
    int traitNum = ckTraits->tokenAsInt(t);
    if (traitNum < 0) continue;
    if (traitNum >= (int) traits.size()) continue;
    Object* trait = traits[traitNum];
    if (!trait) continue; // This should be impossible.
    for (int i = 0; i < 5; ++i) ckChar->resetLeaf(attribNames[i], ckChar->safeGetInt(attribNames[i]) + trait->safeGetInt(attribNames[i]));
  }
}

double WorkerThread::getCkWeight (Object* province, WeightType wtype) {
  string cacheword = "totalTax";   // Keyword used to store the result in the province
  string valueword = "tax_income"; // Keyword for determining raw value of a building
  string modword   = "NOTHING";    // Keyword for getting percentage modifiers

  if (ManPower == wtype) {
    cacheword = "totalMp";
    valueword = "NOTHING";
    modword   = "levy_size";
  }
  else if (FortLevel == wtype) {
    cacheword = "totalFortLevel";
    valueword = "fort_level";
  }
  else if (Galleys == wtype) {
    cacheword = "totalGalleys";
    valueword = "galleys"; 
  }
  
  double ret = province->safeGetFloat(cacheword, -1);
  if (ret > 0) return ret;
  ret = 0;
  double modifier = 1; 

  objvec leaves = province->getLeaves();
  for (objiter holding = leaves.begin(); holding != leaves.end(); ++holding) {
    string htype  = (*holding)->safeGetString("type", "NONE");
    if (htype == "NONE") continue; 
    Object* hinfo = staticMods->safeGetObject(htype);
    if (!hinfo) {
      Logger::logStream(Logger::Warning) << "Warning: Unknown holding type " << htype << " - cannot use for weight.\n";
      continue;
    }
    
    Object* buildList = ckBuildings->safeGetObject(htype);
    if (!buildList) continue;

    if (ManPower == wtype) ret += getManpower(hinfo); 
    else ret += hinfo->safeGetFloat(valueword);
    
    objvec buildings = buildList->getLeaves();
    for (objiter building = buildings.begin(); building != buildings.end(); ++building) {
      if ((*holding)->safeGetString((*building)->getKey(), "no") != "yes") continue;

      if (ManPower == wtype) ret += getManpower(*building);
      else ret += (*building)->safeGetFloat(valueword);
      modifier += (*building)->safeGetFloat(modword); 
    }
  }

  unsigned int div = ckProvToEuProvsMap[province].size();
  if (0 == div) ret = 0.001; // Should be impossible!
  else ret /= div;

  ret *= modifier; 
  province->resetLeaf(cacheword, ret);
  return ret; 
}

double WorkerThread::getManpower (Object* building) {
  double ret = 0;
  ret += 0.5 * building->safeGetFloat("light_infantry");
  ret += 1.0 * building->safeGetFloat("archers");
  ret += 1.0 * building->safeGetFloat("light_cavalry");
  ret += 1.0 * building->safeGetFloat("heavy_infantry");
  ret += 1.0 * building->safeGetFloat("heavy_cavalry");
  ret += 1.0 * building->safeGetFloat("horse_archers");
  ret += 1.0 * building->safeGetFloat("pikemen");
  return ret; 
}

double WorkerThread::getTotalCkWeight (Object* euCountry, WeightType w) {
  double ret = 0; 
  for (objiter prov = euCountryToCkProvincesMap[euCountry].begin(); prov != euCountryToCkProvincesMap[euCountry].end(); ++prov) {
    ret += getCkWeight(*prov); 
  }
  return ret; 
}

void WorkerThread::printTraits () {
  int counter = 0;
  for (objiter trait = traits.begin(); trait != traits.end(); ++trait) {
    Logger::logStream(Logger::Game) << (*trait)->getKey() << " : " << counter++ << "\n"; 
  }
}

void WorkerThread::recursiveAddToHre (Object* ckRuler, Object* euCountry, objvec& electors, objvec& done) {
  if (find(done.begin(), done.end(), euCountry) != done.end()) return; 
  Logger::logStream(DebugHre) << "Adding " << euCountry->getKey() << " to HRE.\n"; 
  done.push_back(euCountry); 
  
  double score = 0;  
  for (objiter prov = euCountryToCkProvincesMap[euCountry].begin(); prov != euCountryToCkProvincesMap[euCountry].end(); ++prov) {
    score += getCkWeight(*prov);
    for (objiter euProv = ckProvToEuProvsMap[*prov].begin(); euProv != ckProvToEuProvsMap[*prov].end(); ++euProv) {
      if (remQuotes((*euProv)->safeGetString("owner")) != euCountry->getKey()) continue;
      Object* history = (*euProv)->getNeededObject("history");      
      (*euProv)->resetLeaf("hre", "yes");
      history->resetLeaf("hre", "yes");
    }
  }

  euCountry->setLeaf("score", score); 
  objiter canBeat = electors.end();
  for (objiter c = electors.begin(); c != electors.end(); ++c) {
    if (score < (*c)->safeGetFloat("score")) continue;
    canBeat = c;
    break;
  }
  electors.insert(canBeat, euCountry);
  if (configObject->safeGetInt("numElectors") < (int) electors.size()) {
    Object* worst = electors.back();
    worst->unsetValue("score");
    electors.pop_back(); 
  }

  for (objiter title = beginRel(ckRuler, Title); title != finalRel(ckRuler, Title); ++title) {
    for (objiter vassal = vassalMap[*title].begin(); vassal != vassalMap[*title].end(); ++vassal) {
      Object* vassalChar = titleToCharMap[*vassal];
      if (!vassalChar) continue;
      Object* vassalNation = characterToEuCountryMap[vassalChar];
      if (!vassalNation) continue;
      recursiveAddToHre(vassalChar, vassalNation, electors, done); 
    }
  }
}

void WorkerThread::recursiveCollectCultures (Object* ckRuler, map<string, double>& weights, int iteration) {
  string ckCulture = remQuotes(ckRuler->safeGetString("culture")); 
  if (cultureMap.find(ckCulture) == cultureMap.end()) {
    Logger::logStream(Logger::Warning) << "Warning: Could not convert culture "
				       << ckCulture
				       << " of character "
				       << ckRuler->getKey()
				       << ".\n"; 
  }
  else {
    // Give some weight for each title, making sure to check whether it is a special area. 
    for (objiter title = beginRel(ckRuler, Title); title != finalRel(ckRuler, Title); ++title) {
      Object* ckTitle = (*title);
      string euCulture = cultureMap[ckCulture];
      double titleWeight = 0;
      switch (titleTier(ckTitle)) {
      case Empire:  titleWeight = configObject->safeGetFloat("e_weight", 1.0); break;
      case Kingdom: titleWeight = configObject->safeGetFloat("k_weight", 1.0); break;
      case Duchy:   titleWeight = configObject->safeGetFloat("d_weight", 1.5); break;
      case County:  titleWeight = configObject->safeGetFloat("c_weight", 2.0); break;
      case Barony:  titleWeight = configObject->safeGetFloat("b_weight", 1.5); break;	
      default:
      case Other:
	titleWeight = 0;
	Logger::logStream(Logger::Warning) << "Warning: Do not recognise tier of title " << ckTitle->getKey() << ", no culture weight assigned.\n";
	break;
      }
      
      while (ckTitle) {
	string dejure = remQuotes(ckTitle->safeGetString("de_jure_liege", "---"));
	// If it is currently under its dejure liege, it's not listed, so check for actual liege as backup. 
	if (dejure == "---") dejure = remQuotes(ckTitle->safeGetString("liege", "---"));
	if (dejure == "---") break; 
	if (specialCultureMap.find(dejure) != specialCultureMap.end()) {
	  string candidate = specialCultureMap[dejure][ckCulture];
	  if (candidate != "") {
	    euCulture = candidate;
	    Logger::logStream(DebugCulture) << "Special-converting culture "
					    << ckCulture
					    << " as "
					    << candidate
					    << " for title "
					    << (*title)->getKey()
					    << " due to de-jure liege "
					    << dejure
					    << ".\n";
	  }
	  break; 
	}
	ckTitle = titleMap[dejure];
      }
      weights[euCulture] += titleWeight / iteration; 
    }
  }
  
  for (objiter vassal = vassalMap[ckRuler].begin(); vassal != vassalMap[ckRuler].end(); ++vassal) {
    recursiveCollectCultures(*vassal, weights, iteration+1); 
  }
}

void WorkerThread::recursiveCollectReligion (Object* ckRuler, map<string, double>& weights, int iteration) {
  string ckReligion = remQuotes(ckRuler->safeGetString("religion")); 
  if (religionMap.find(ckReligion) == religionMap.end()) {
    Logger::logStream(Logger::Warning) << "Warning: Could not convert religion "
				       << ckReligion
				       << " of character "
				       << ckRuler->getKey()
				       << ".\n"; 
  }
  else {
    // Give some weight for each title, making sure to check whether it is a special area. 
    for (objiter title = beginRel(ckRuler, Title); title != finalRel(ckRuler, Title); ++title) {
      Object* ckTitle = (*title);
      string euReligion = religionMap[ckReligion];
      double titleWeight = 0;
      switch (titleTier(ckTitle)) {
      case Empire:  titleWeight = configObject->safeGetFloat("e_weight", 1.0); break;
      case Kingdom: titleWeight = configObject->safeGetFloat("k_weight", 1.0); break;
      case Duchy:   titleWeight = configObject->safeGetFloat("d_weight", 1.5); break;
      case County:  titleWeight = configObject->safeGetFloat("c_weight", 2.0); break;
      case Barony:  titleWeight = configObject->safeGetFloat("b_weight", 1.5); break;		
      default:
      case Other:
	titleWeight = 0;
	Logger::logStream(Logger::Warning) << "Warning: Do not recognise tier of title " << ckTitle->getKey() << ", no religion weight assigned.\n";
	break;
      }
      
      weights[euReligion] += titleWeight / iteration; 
    }
  }
  
  for (objiter vassal = vassalMap[ckRuler].begin(); vassal != vassalMap[ckRuler].end(); ++vassal) {
    recursiveCollectReligion(*vassal, weights, iteration+1); 
  }
}

void WorkerThread::setCharacterAttributes (Object* euMonarch, Object* ckRuler, int monarchId, Object* dummyBestChar, Object* dummyWorstChar) {
  static Object* dummyTempChar  = new Object("dummyTemp"); 
  
  euMonarch->resetLeaf("name", ckRuler->safeGetString("birth_name", "NEMO"));

  for (unsigned int a = 0; a < attribNames.size(); ++a) {
    double percentage = ckRuler->safeGetFloat(attribNames[a]) - dummyWorstChar->safeGetFloat(attribNames[a]);
    percentage /= (0.0001 + dummyBestChar->safeGetFloat(attribNames[a]));
    dummyTempChar->resetLeaf(attribNames[a], percentage);
  }

    
  double adm = 3 + 3 * (dummyTempChar->safeGetFloat("stewardship") + dummyTempChar->safeGetFloat("learning"));
  if (adm > 9) adm = 9;
  if (adm < 3) adm = 3;  
  euMonarch->resetLeaf("ADM", (int) floor(adm + 0.5));
  adm = 3 + 6 * dummyTempChar->safeGetFloat("martial");
  if (adm > 9) adm = 9;
  if (adm < 3) adm = 3;        
  euMonarch->resetLeaf("MIL", (int) floor(adm + 0.5));
  adm = 3 + 3 * (dummyTempChar->safeGetFloat("diplomacy") + dummyTempChar->safeGetFloat("intrigue"));
  if (adm > 9) adm = 9;
  if (adm < 3) adm = 3;        
  euMonarch->resetLeaf("DIP", (int) floor(adm + 0.5));

  Logger::logStream(DebugLeaders) << "Converted "
				  << euMonarch->safeGetString("name")
				  << " with ADM "
				  << euMonarch->safeGetString("ADM") << "-"
				  << euMonarch->safeGetString("DIP") << "-"
				  << euMonarch->safeGetString("MIL") << " from CK stats";
  for (unsigned int a = 0; a < attribNames.size(); ++a) 
    Logger::logStream(DebugLeaders) << " " << ckRuler->safeGetString(attribNames[a]) << " ("
				    << dummyTempChar->safeGetString(attribNames[a]) << ")";

  Logger::logStream(DebugLeaders) << ".\n"; 
    
  Object* id = new Object("id");
  id->resetLeaf("id", monarchId);
  id->resetLeaf("type", "37");
  euMonarch->setValue(id);

  Object* dynasty = dynasties[ckRuler->safeGetString("dynasty")];
  string dynastyName = "\"Unknown Dynasty\"";
  if (dynasty) dynastyName = dynasty->safeGetString("name");
  else Logger::logStream(DebugLeaders) << "Warning: Could not find dynasty "
				       << ckRuler->safeGetString("dynasty")
				       << " for monarch "
				       << ckRuler->safeGetString("birth_name")
				       << ".\n"; 
  euMonarch->resetLeaf("dynasty", dynastyName); 
}

/******************************* End calculators ********************************/


void WorkerThread::convertEU4 () {
  if (!ck2Game) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return; 
  }
  targetVersion = configObject->safeGetString("eu4dir", targetVersion); 

  
  Logger::logStream(Logger::Game) << "Loading EU source file.\n";
  euxGame = loadTextFile(targetVersion + "input.eug");
  
  Logger::logStream(Logger::Game) << "Done with conversion, writing to file.\n"; 
  ofstream writer;
  writer.open(".\\Output\\converted.eug");
  Parser::topLevel = euxGame;
  writer << (*euxGame);
  writer.close();
  Logger::logStream(Logger::Game) << "Done writing.\n";  
}

void WorkerThread::convertEU3 () {
  if (!ck2Game) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return; 
  }

  string debuglog = configObject->safeGetString("logfile");
  if (debuglog != "") {
    string outputdir = "Output\\";
    debuglog = outputdir + debuglog;

    DWORD attribs = GetFileAttributesA(debuglog.c_str());
    if (attribs != INVALID_FILE_ATTRIBUTES) {
      int error = remove(debuglog.c_str());
      if (0 != error) Logger::logStream(Logger::Warning) << "Warning: Could not delete old log file. New one will be appended.\n";
    }
    parentWindow->writeDebugLog(debuglog);
  }

  
  Logger::logStream(Logger::Game) << "Loading EU3 source file.\n";
  euxGame = loadTextFile(targetVersion + "input.eu3");

  initialiseCharacters(); 
  initialiseRelationMaps();
  attribNames.push_back("diplomacy");
  attribNames.push_back("martial");  
  attribNames.push_back("stewardship");
  attribNames.push_back("intrigue");  
  attribNames.push_back("learning"); 
  
  loadFiles();
  createProvinceMap(); 
  createVassalsMap();
  createCountryMap(); 
  assignCKprovinces();

  printTraits(); 
  eu3Provinces();
  eu3Histories(); 
  eu3Diplomacy();
  eu3Governments(); 
  eu3Taxes();
  eu3Manpower(); 
  eu3StateVariables();
  eu3ProvinceCultures();
  eu3ProvinceReligion();   
  eu3StateCultures();
  eu3StateReligion();   
  eu3Characters(); 
  eu3Cores(); 
  eu3Sliders(); 
  eu3Armies(); 
  eu3Hre();
  eu3Cots(); 
  eu3Papacy(); 
  eu3Techs(); 
  
  Logger::logStream(Logger::Game) << "Done with conversion, writing to file.\n";
  DWORD attribs = GetFileAttributesA("Output");
  if (attribs == INVALID_FILE_ATTRIBUTES) {
    Logger::logStream(Logger::Warning) << "Warning, no Output directory, attempting to create one.\n";
    int error = _mkdir("Output");
    if (-1 == error) {
      Logger::logStream(Logger::Error) << "Error: Could not create Output directory. Cannot write to file.\n";
      return; 
    }
  }
 
  ofstream writer;
  writer.open(".\\Output\\converted.eu3");
  Parser::topLevel = euxGame;
  writer << (*euxGame);
  writer.close();
  Logger::logStream(Logger::Game) << "Done writing.\n";
  parentWindow->closeDebugLog(); 
}
