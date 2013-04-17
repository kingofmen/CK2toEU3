#include "Ck2Window.hh"
#include <QPainter> 
#include "Parser.hh"
#include <cstdio> 
#include <QtGui>
#include <QDesktopWidget>
#include <QRect>
#include <qpushbutton.h>
#include <iostream> 
#include <fstream> 
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

void CleanerWindow::writeDebugLog (string fname) {
  if (fname == "") return; 
  QFile outfile;
  outfile.setFileName(QString(fname.c_str()));
  outfile.open(QIODevice::Append | QIODevice::Text);
  QTextStream out(&outfile);
  out << textWindow->toPlainText() << endl;
  outfile.close(); 
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

void WorkerThread::getStatistics () {
  if (!ck2Game) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return; 
  }

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
      //Logger::logStream(Logger::Warning) << "Warning: Title " << (*title)->getKey() << " not held by anyone.\n";
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
}

void WorkerThread::eu3Diplomacy () {
  Object* eu3Diplomacy = euxGame->safeGetObject("diplomacy");
  assert(eu3Diplomacy);
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
  for (map<Object*, objvec>::iterator link = euProvToCkProvsMap.begin(); link != euProvToCkProvsMap.end(); ++link) {
    Object* eup = (*link).first;
    objvec ckps = (*link).second;
    if (0 == ckps.size()) {
      Logger::logStream(Logger::Warning) << "Warning: No CK provinces for "
					 << eup->getKey()
					 << ", no reassignment made.\n";
      continue; 
    }

    
    map<Object*, double> own_weights;
    map<Object*, double> con_weights;
    double owner_con_weight = 0;    
    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
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

    Object* history = eup->getNeededObject("history");    
    eup->resetLeaf("owner", addQuotes(winner->getKey()));
    eup->resetLeaf("controller", conTag == "" ? addQuotes(winner->getKey()) : conTag);
    history->resetLeaf("owner", addQuotes(winner->getKey()));
    history->resetLeaf("controller", conTag == "" ? addQuotes(winner->getKey()) : conTag);

    for (objiter ckp = ckps.begin(); ckp != ckps.end(); ++ckp) {
      if (find(euCountryToCkProvincesMap[winner].begin(), euCountryToCkProvincesMap[winner].end(), (*ckp)) != euCountryToCkProvincesMap[winner].end()) continue; 
      euCountryToCkProvincesMap[winner].push_back(*ckp);
    }
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
  }

  double minimumGold = configObject->safeGetFloat("minimumGold", 10); 
  for (map<Object*, Object*>::iterator i = euCountryToCharacterMap.begin(); i != euCountryToCharacterMap.end(); ++i) {
    Object* euCountry = (*i).first;
    Object* ckRuler   = (*i).second;

    double gold = max(ckRuler->safeGetFloat("wealth"), 0.0); 
    double prestige = ckRuler->safeGetFloat("prestige") + ckRuler->safeGetFloat("piety");

    gold /= totalCkGold;
    gold *= totalEuGold;
    gold = max(gold, minimumGold); 
    euCountry->resetLeaf("treasury", gold);

    prestige /= maxCkPrestige;
    prestige *= maxEuPrestige;
    euCountry->resetLeaf("precise_prestige", prestige); 
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
  string cacheword = "totalTax";
  string valueword = "tax_income";
  string modword   = "NOTHING";

  if (ManPower == wtype) {
    cacheword = "totalMp";
    valueword = "NOTHING";
    modword   = "levy_size";
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

    
  double adm = 4.5 * (dummyTempChar->safeGetFloat("stewardship") + dummyTempChar->safeGetFloat("learning"));
  if (adm > 9) adm = 9;
  if (adm < 0) adm = 0;  
  euMonarch->resetLeaf("ADM", (int) floor(adm + 0.5));
  adm = 9 * dummyTempChar->safeGetFloat("martial");
  if (adm > 9) adm = 9;
  if (adm < 0) adm = 0;        
  euMonarch->resetLeaf("MIL", (int) floor(adm + 0.5));
  adm = 4.5 * (dummyTempChar->safeGetFloat("diplomacy") + dummyTempChar->safeGetFloat("intrigue"));
  if (adm > 9) adm = 9;
  if (adm < 0) adm = 0;        
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

  eu3Provinces(); 
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

  string debuglog = configObject->safeGetString("logfile");
  if (debuglog != "") {
    string outputdir = "Output\\";
    debuglog = outputdir + debuglog;

    attribs = GetFileAttributesA(debuglog.c_str());
    if (attribs != INVALID_FILE_ATTRIBUTES) {
      int error = remove(debuglog.c_str());
      if (0 != error) Logger::logStream(Logger::Warning) << "Warning: Could not delete old log file. New one will be appended.\n";
    }
    

    
    parentWindow->writeDebugLog(debuglog);
  }
  
  ofstream writer;
  writer.open(".\\Output\\converted.eu3");
  Parser::topLevel = euxGame;
  writer << (*euxGame);
  writer.close();
  Logger::logStream(Logger::Game) << "Done writing.\n";  
}
