#ifndef CLEANERWINDOW_HH
#define CLEANERWINDOW_HH

#include <QtGui>
#include <QObject>
#include <QThread> 
#include "Object.hh"
#include <map>

using namespace std;

class WorkerThread; 
class PopMerger; 

class CleanerWindow : public QMainWindow {
  Q_OBJECT
  
public:
  CleanerWindow (QWidget* parent = 0); 
  ~CleanerWindow ();
  
  QPlainTextEdit* textWindow; 
  WorkerThread* worker;
  
  void loadFile (string fname, int autoTask = -1);
						      
public slots:

  void loadFile (); 
  void cleanFile ();
  void getStats ();
  void convertEU3 ();
  void convertEU4 ();
  void colourMap (); 
  void message (QString m); 
  
private:

};

struct ObjectSorter {
  ObjectSorter (string k) {keyword = k;} 
  string keyword;
};
struct ObjectAscendingSorter : public ObjectSorter {
public:
  ObjectAscendingSorter (string k) : ObjectSorter(k) {}
  bool operator() (Object* one, Object* two) {return (one->safeGetFloat(keyword) < two->safeGetFloat(keyword));} 
private:
};
struct ObjectDescendingSorter : public ObjectSorter {
public:
  ObjectDescendingSorter (string k) : ObjectSorter(k) {}
  bool operator() (Object* one, Object* two) {return (one->safeGetFloat(keyword) > two->safeGetFloat(keyword));} 
private:
};

class WorkerThread : public QThread {
  Q_OBJECT
public:
  WorkerThread (string fname, int aTask = -1); 
  ~WorkerThread ();

  enum TaskType {LoadFile = 0,
		 CleanFile,
		 Statistics,
		 ConvertEu3,
		 ConvertEu4,
		 ColourMap,
		 NumTasks}; 
  void setTask(TaskType t) {task = t;} 

protected:
  void run (); 
  
private:
  // Misc globals 
  string targetVersion;
  string sourceVersion; 
  string fname; 
  Object* euxGame;
  Object* ck2Game;
  TaskType task; 
  Object* configObject;
  int autoTask;
  
  // Infrastructure 
  void loadFile (string fname); 
  void cleanFile ();
  void getStatistics ();
  void convertEU3 ();
  void convertEU4 ();   
  void configure ();
  void colourMap (); 

  // Initialisers
  void assignCKprovinces ();
  void createCountryMap ();
  void createProvinceMap ();  
  void createVassalsMap ();
  void initialiseCharacters ();
  void initialiseRelationMaps ();
  void loadFiles (); 

  // Eu3 conversions
  void eu3Characters ();
  void eu3Diplomacy ();
  void eu3Governments ();     
  void eu3Manpower ();
  void eu3ProvinceCultures ();
  void eu3ProvinceReligion ();    
  void eu3Provinces ();
  void eu3StateCultures ();
  void eu3StateReligion ();   
  void eu3StateVariables ();
  void eu3Taxes ();  

  // Calculators, helper methods
  enum WeightType {BaseTax, ManPower, NumWeights};
  void calculateAttributes (Object* ckChar);  
  double getCkWeight (Object* ckprov, WeightType = BaseTax);
  double getManpower (Object* building);
  void recursiveCollectCultures (Object* ckRuler, map<string, double>& weights, int iteration);
  void recursiveCollectReligion (Object* ckRuler, map<string, double>& weights, int iteration);
  void setCharacterAttributes (Object* euMonarch, Object* ckRuler, int monarchId, Object* dummyBestChar, Object* dummyWorstChar); 
  
  enum TitleTier {Empire, Kingdom, Duchy, County, Barony, Other};
  enum IterType {Chars, Titles};
  enum RelationType {Father, Mother, Son, Daughter, Child, Title};
  
  // Iterators:
  objiter begin (IterType it = Chars); 
  objiter final (IterType it = Chars); 

  // Uplookers
  Object* getChar (int id) {return characterIndex[id];}
  Object* getChar (string id) {return getChar(atoi(id.c_str()));}
  Object* getTitle (string id) {return titleMap[id];}

  objiter beginRel (Object* c, RelationType r); 
  objiter finalRel (Object* c, RelationType r);
  
  // Helpers: 
  Object* loadTextFile (string fname);
  TitleTier titleTier (Object* dat); 
  
  // CK2 game objects
  objvec characters;
  objvec titles;
  objvec ck2provs; 
  map<int, Object*> characterIndex; // Use map because vector runs out of memory! (Or anyway causes strange crash on large resize.) 

  // Family relationships
  map<Object*, objvec> fathers;
  map<Object*, objvec> mothers; 
  map<Object*, objvec> children;
  map<Object*, objvec> daughters;
  map<Object*, objvec> sons;

  // Titles, land
  map<Object*, objvec> charTitles; 
  map<Object*, Object*> titleToCharMap;
  map<Object*, Object*> euCountryToCharacterMap;
  map<Object*, Object*> euCountryToCkCountryMap; 
  map<Object*, Object*> characterToEuCountryMap; 
  
  // String lookups
  map<string, Object*> titleMap;
  vector<string> attribNames; 
  
  // Countries
  objvec sovereigns;
  map<Object*, Object*> liegeMap;
  map<Object*, objvec> vassalMap;

  // Input info
  objvec countryLinks;
  objvec provinceLinks;
  map<Object*, objvec> ckProvToEuProvsMap;
  map<Object*, objvec> euProvToCkProvsMap;
  Object* ckBuildings;
  Object* staticMods;
  map<string, string> religionMap;
  map<string, string> cultureMap;
  map<string, map<string, string> > specialCultureMap;
  objvec traits;
  map<string, Object*> dynasties; 
}; 

#endif

