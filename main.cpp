#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <vector>
#include <sys/ioctl.h>
#include <pthread.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

struct Feed {
  string title;
  vector<string> content;
  string completeFeed;
  string url;
  bool lock;
};

struct Thread_Data {
  Feed* to_update;
  int initSleep;
};

struct Entry {
  string source;
  string title;
};

Feed* parseRSS(const vector<string> &lineArray);
void trimFeed(vector<Feed*> aFeed);
void trimFeedRich(vector<Feed*> aFeed);
vector<string> getFeed(string url);
string getNewsChunk(int &start, int size, const string &text);
void checkWidth();
void *keepUpdated(Thread_Data* requiredData);
void *monitorNewEntry(void* useless);
vector<string> listTag(string inputFile);
void listFeed(vector<Feed*> aFeed);
int width;
int height;
unsigned int updateInterval = 1; /*In minutes*/
vector<Entry> recentEntryList;
bool recentEntryListLock = false;

char helpStr[] = "RSS viewer 0.2:\n\
rssViewer [CONTINOUS|SPLITTED] [feed(s )source(s)] ...\n\
  --quiet Do not use the stock-like output, print only in files\n\
  --nofile Do not send news to a temporary file\n\
  --list Display a list of entry instead of the stock view\n\
  --onepass Quit once the news have been displayed\n\n\
Copyright Emmanuel Lepage Vallee (2009)\n";

/*Options*/
bool quiet=false;
bool dontUseFile=false;
bool printList=false;
bool quitAfter=false;

int main(int argc, char *argv[]) {
  checkWidth();
  
  if (argc < 2) {
    printf("%s",helpStr);
    exit(0);
  }
  
  for (int i=0;i< argc; i++) 
    if (string(argv[i]) == "--quiet")
      quiet=true;
    else if (string(argv[i]) == "--nofile")
      dontUseFile=true;
    else if (string(argv[i]) == "--list")
      printList=true;
    else if (string(argv[i]) == "--onepass")
      quitAfter=true;
    else if (string(argv[i]) == "--version") {
      printf("0.2\n");
      exit(0);
    }
    else if (string(argv[i]) == "--help") {
      printf("%s",helpStr);
      exit(0);
    }
    
  
  pthread_t aTread;
  if (!dontUseFile)
    pthread_create(&aTread,NULL,(void* (*)(void*))monitorNewEntry,0);
  
  vector<string> lineList;
  Feed* aFeed;
  vector<Feed*> feedVector;

  for (int i =1;i < argc;i++) {
    lineList = getFeed(argv[i]);
    aFeed = parseRSS(lineList);
    aFeed->url = argv[i];
    feedVector.push_back(aFeed);
    aFeed->lock = false;
    
    pthread_t aTread;
    Thread_Data* aTreadData = new Thread_Data;
    aTreadData->to_update = aFeed;
    aTreadData->initSleep = ((i-1)*(updateInterval/argc-1))*60; //TODO need update if option added
    pthread_create(&aTread,NULL,(void* (*)(void*))keepUpdated,aTreadData);
  }
  
  if (printList)
    listFeed(feedVector);
  else if (!quiet)
    trimFeedRich(feedVector);
  else
    while (true) sleep(1000);
  return 0;
}

Feed* parseRSS(const vector<string> &lineArray) {
  string allText;
  for (int i =0; i < lineArray.size();i++)
    allText += lineArray[i];
  vector<string> parsedText = listTag(allText);
  vector<string> newsList;
  string entry;
  string title;
  bool ifTitle = true;
  
  for (int i=0; i < parsedText.size();i++) {
    entry.clear();
    if (i >= parsedText.size())
      break;
    if (parsedText[i].find("<title>") != -1) {
	if (parsedText[i].find("</title>") != -1)
	  if (parsedText[i].size() >= (parsedText[i].find("<title>")+7)+(parsedText[i].find("</title>")- (parsedText[i].find("<title>")+7)))
	    entry = parsedText[i].substr((parsedText[i].find("<title>")+7), (parsedText[i].find("</title>")- (parsedText[i].find("<title>")+7)));
	else 
	  if (parsedText[i].size() >= (parsedText[i].find("<title>")+7)+(parsedText[i].size() - (parsedText[i].find("<title>")+7)))
	    entry = parsedText[i].substr((parsedText[i].find("<title>")+7), (parsedText[i].size() - (parsedText[i].find("<title>")+7)));
	while (parsedText[i].find("</title>") == -1) {
	  i++;
	  if (i >= parsedText.size())
	    break;
	  if (parsedText[i].find("</title>") == -1) 
	    entry += " " + parsedText[i];
	  else if (parsedText[i].find("</title>") != 0)
	    if (parsedText[i].size() >= parsedText[i].find("</title>"))
	      entry += " " + parsedText[i].substr(0, (parsedText[i].find("</title>")));
	  else
	    if (parsedText[i].size() >= parsedText[i].size() -8)
	      entry += " " + parsedText[i].substr(0, (parsedText[i].size() -8));
	}

	if (ifTitle == true){
	  title = entry;
	  ifTitle = false;
	}
	else 
	  newsList.push_back(entry);
    }
  }
  
  Feed* aFeed = new Feed;
  aFeed->title = title;
  aFeed->content = newsList;
  for (int j=0; j < aFeed->content.size();j++)
    aFeed->completeFeed += aFeed->content[j] + " - ";
  return aFeed;
}

void trimFeed(vector<Feed*> aFeed) {
  int start = 0;
  int* startArray = new int[aFeed.size()];
  for (int i =0;i<aFeed.size();i++)
    startArray[i] =0;
  
  while (1) {
    system("clear");
    for (int i =0;i<aFeed.size();i++) {
      checkWidth();
      unsigned int newsWidth = width - aFeed[i]->title.size() - 6;
      printf ("[ %s ]: %s\n",aFeed[i]->title.c_str(),getNewsChunk(startArray[i],newsWidth,aFeed[i]->completeFeed).c_str());
    }
    for (int i =0;i<aFeed.size();i++)
      startArray[i] += 2;
    sleep (1);
  }
}

void trimFeedRich(vector<Feed*> aFeed) {
  string alt1 = (aFeed.size() == 1)?"\033[00;00m":"\033[00;32m";
  string alt2 = (aFeed.size() == 1)?"\033[00;00m":"\033[0;33m";
  
  int* startArray = new int[aFeed.size()];
  for (int i =0;i<aFeed.size();i++)
    startArray[i] =0;
  
  int* startArray2 = new int[aFeed.size()];
  for (int i =0;i<aFeed.size();i++)
    startArray2[i] =0;
  
  bool increment = true;
  while (1) {
    system("clear");
    char halfLine[((width/2) - 4)];
    for (int i=0;i<((width/2) - 4);i++)
      halfLine[i] = ' ';
    halfLine[((width/2) - 4)] = '\0';
    printf("\033[30;47m%sRSS feed%s\033[31;00m\n",halfLine,halfLine);
    for (int i =0;i<aFeed.size();i++) {
      checkWidth();
      unsigned int newsWidth = width - aFeed[i]->title.size() - 6;
      while (aFeed[i]->lock) sleep(1);
      aFeed[i]->lock = true;
      printf ("\033[00;00m\033[01;29m[ %s ]: %s%s\n",aFeed[i]->title.c_str(),((i+1)%2 == 1)?alt1.c_str():alt2.c_str(),getNewsChunk((((i%2) == 1)?startArray2[i]:startArray[i]),newsWidth,aFeed[i]->completeFeed).c_str());
      aFeed[i]->lock = false;
    }
    increment = !increment;
    for (int i =0;i<aFeed.size();i++) {
      if (increment) 
	startArray[i] += 1;
      startArray2[i] += 1;
    }
    
    /*Recent news*/
    printf("\033[00;00m\n\n\033[01;29mMost recent news:\033[31;00m\n");
    int freeLineNb(height-aFeed.size()-5), blankLineNB;
    while (recentEntryListLock == true) sleep(1);
    recentEntryListLock = true;
    if (recentEntryList.size() < freeLineNb - 1 ) {
      blankLineNB = freeLineNb - recentEntryList.size();
      for (int i =0;i<recentEntryList.size();i++) 
	printf(" -%s (%s)\n",recentEntryList[i].title.c_str(),recentEntryList[i].source.c_str());
    }
    else {
      blankLineNB = 1;
      for (int i =0;i<freeLineNb - 1;i++) 
	printf(" -%s (%s)\n",recentEntryList[i].title.c_str(),recentEntryList[i].source.c_str());
    }
    
    recentEntryListLock = false;
    
    /*Bottom bar*/
    int textWidth = strlen("RSS Stock like viewer 0.2") + strlen("(c)Emmanuel Lepage Vallee (2009) ");
    char jumpLine[blankLineNB];
    for (int i=0;i<blankLineNB;i++)
      jumpLine[i] = '\n';
    jumpLine[blankLineNB] = '\0';
    char lineBottom[width - textWidth];
    for (int i=0;i<(width - textWidth);i++)
      lineBottom[i] = ' ';
    lineBottom[(width - textWidth)] = '\0';
    printf("%s\033[30;47mRSS Stock like viewer 0.2%s(c)Emmanuel Lepage Vallee (2009) \033[31;00m",jumpLine,lineBottom);
    fflush(stdout);
    if (!quitAfter)
      usleep (400000); //200000
    else
      exit(0);
    //sleep(1);
  }
}

void listFeed(vector<Feed*> aFeed) {
  while (1) {
    for (int i =0;i<aFeed.size();i++) {
      for (int j=0;j < aFeed[i]->content.size();j++)
	printf("%s\n",aFeed[i]->content[j].c_str());
    }
    if (!quitAfter)
      sleep(30 * 60); /*Half hours*/
    else
      exit(0);
  }
}

string getNewsChunk(int &start, int size, const string &text) {
  string shotStr;
  if (start >= (text.size()-2)) 
    start =0; //BUG
  if(((start, (size - ((start + size) - text.size()))) < text.size()) && (((start + size) - text.size())) < text.size())
    shotStr = (text.substr(start, (size - ((start + size) - text.size()))) + (text.substr(0, ((start + size) - text.size()))));
  else 
    if (text.size() >= start+size)
      shotStr = text.substr(start, size);
  return shotStr;
}

vector<string> getFeed(string url) {
  pid_t pid, pidchild;
  int to_me[2], from_me[2];
  string tmpStr;
  pipe(to_me);
  pipe(from_me);
  if ((pid = fork()) == 0) {
      dup2(to_me[0], STDIN_FILENO);
      dup2(from_me[1], STDOUT_FILENO);
      close(to_me[0]);
      close(to_me[1]);
      close(from_me[0]);
      close(from_me[1]);
      CURL *curl;
      CURLcode res;
      curl = curl_easy_init();
      if(curl) {
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tmpStr);
      }
      exit(33);
  }
  char bufferR[1024];
  ssize_t nr;
  close(from_me[1]);
  close(to_me[0]);
  vector<string> lineList;
  lineList.push_back("");
  int i =0;
  
  do {
    nr = read(from_me[0], bufferR, sizeof bufferR);
    for (i = 0; i < nr; i++) 
      if (bufferR[i] == '\n')
	lineList.push_back("");
      else
	lineList[lineList.size()-1] += bufferR[i];
  } while (nr > 0);
  int returnCode =0;
  wait(&returnCode);
  
  close(to_me[0]);
  close(to_me[1]);
  close(from_me[0]);
  close(from_me[1]);
  
  return lineList;
}

void checkWidth() {
  struct winsize w;
  ioctl(0, TIOCGWINSZ, &w);
  width = w.ws_col;
  height = w.ws_row;
}

void *keepUpdated(Thread_Data* requiredData) {
  if (requiredData->initSleep < 0)
    requiredData->initSleep = -requiredData->initSleep;
  sleep(requiredData->initSleep);

  while(true) {
    vector<string> lineList = getFeed(requiredData->to_update->url);
    Feed* aFeed = parseRSS(lineList);
    for (int i =0; i < aFeed->content.size();i++) {
      if (aFeed->content[i] != requiredData->to_update->content[0]) {
	while (recentEntryListLock == true) sleep(1);
	recentEntryListLock = true;
	Entry anEntry = {requiredData->to_update->title, aFeed->content[i]};
	recentEntryList.insert(recentEntryList.begin(),anEntry);
	recentEntryListLock = false;
      }
      else
	break;
    }
    requiredData->to_update->content = aFeed->content;
    while (requiredData->to_update->lock) sleep(1);
    requiredData->to_update->lock = true;
    requiredData->to_update->completeFeed = aFeed->completeFeed;
    requiredData->to_update->lock = false;
    if (updateInterval < 3)
      updateInterval = 3;
    sleep(updateInterval * 60);
  }
}

void *monitorNewEntry(void* useless) {
  int previousSize = 0;
  while (true) {
    while (recentEntryListLock == true) sleep(1);
    recentEntryListLock = true;
    int test44 = recentEntryList.size();
    if (test44 > previousSize) {
      system("clear");
      FILE * pFile;
      pFile = fopen ("/tmp/newRssEntry.txt","a");
      for (int i=0; i< recentEntryList.size() - previousSize;i++) {
	fputs (recentEntryList[i].source.c_str(),pFile);
	fputc ('\n',pFile);
	fputs (recentEntryList[i].title.c_str(),pFile);
	fputc ('\n',pFile);
      }
      fclose (pFile);
      previousSize = recentEntryList.size();
    }
    recentEntryListLock = false;
    if (recentEntryList.size() > 100)
     recentEntryList.clear(); 
    sleep(270);
  }
}

void split(vector<string> &tagList, string &inputFile, uint index) {
  tagList.push_back(inputFile.substr(0,index));
  inputFile = inputFile.erase(0,index);
}

vector<string> listTag(string inputFile) {
  while (inputFile.find("&#60;") !=-1)
    inputFile.replace(inputFile.find("&#60;"),5,"<");
  while (inputFile.find("&#xB0;") !=-1)
    inputFile.replace(inputFile.find("&#xB0;"),6,""); /*Prevent formatting error*/
  vector<string> tagList;
  while (inputFile != "") {
    if (inputFile.size() > 0)
      while ((inputFile[0] == 0x20/*space*/) || (inputFile[0] == 0x09/*tab*/) || (inputFile[0] == 0x0A/*line break*/))
	inputFile = inputFile.erase(0,1);
    else
      break;
    if ((inputFile.find("<") == -1) || (inputFile.find(">") == -1))
      split(tagList, inputFile, inputFile.size());    
    else if (inputFile.find("<") < inputFile.find_last_of("<",inputFile.find(">"))) 
      split(tagList, inputFile, inputFile.find_last_of("<",inputFile.find(">")-1));
    else if (inputFile.substr(0,4) == "<!--") 
      split(tagList, inputFile, inputFile.find("->")+2);
    else if (inputFile.find("<") == 0) 
      split(tagList, inputFile, inputFile.find(">")+1);
    else if (inputFile != "") 
      split(tagList, inputFile, inputFile.find("<"));
  }
  
  //for (int i=0;i<tagList.size();i++)
   // printf("%s\n\n",tagList[i].c_str());
  
  return tagList;
}