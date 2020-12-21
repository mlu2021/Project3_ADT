//  PROJECT IDENTIFIER: 01BD41C3BF016AD7E8B6F837DF18926EC3E83350
//  main.cpp
//  Project 1
//
//  Created by Michelle Lu on 9/12/19.
//  Copyright © 2019 Michelle Lu. All rights reserved.
//

#include <stdio.h>
#include <vector>
#include <stack>
#include <queue>
#include <deque>
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <math.h>
#include "xcode_redirect.hpp"

using namespace std;

// TODO: shorten the variable names?
struct LogEntry {
    size_t entryID;
    int64_t time;
    string timeField;
    string category;
    string message;
};

// conparator for log entries
struct CmpLogEntries {
    bool operator()(const LogEntry& lhs, const LogEntry& rhs) const {
        if (lhs.time == rhs.time) {
            if (lhs.category == rhs.category) {
                return lhs.entryID < rhs.entryID;
            }
            return lhs.category < rhs.category;
        }
        return lhs.time < rhs.time;
    }
};

// conparator for log entries
struct CmpLogEntriesPtrs {
    //should lhs, rhs be passed by reference or by value?
    bool operator()(const LogEntry* lhs, const LogEntry* rhs) const {
        if (lhs->time == rhs->time) {
            if (lhs->category == rhs->category) {
                return lhs->entryID < rhs->entryID;
            }
            return lhs->category < rhs->category;
        }
        return lhs->time < rhs->time;
    }
};

struct CompTime {
    bool operator()(const LogEntry &entry, const int64_t &ts) const {
        return entry.time < ts;
    }
    
    bool operator()(const int64_t ts, const LogEntry &entry) const {
        return ts < entry.time;
    }
};

// master log entries, sorted based on timestamp
vector<LogEntry> logEntries;

// a hash map to hold category keywords
unordered_map<string, vector<LogEntry*>> categoryMap;

// a hash map to hold all keywords
unordered_map<string, vector<LogEntry*>> keywordMap;

// a deck to hold the excerpt list
deque<LogEntry*> excerptList;

// a vector to hold the last search results
vector<LogEntry*> searchResults;

// a variable to indicate whether search has even been performed
bool searchPerformed = false;

/**
 * GetOpts
 *
 */
void get_options(int argc, char** argv) {
    
    int option_index = 0, option = 0;
    opterr = false;
    
    struct option longOpts[] = {{"help", no_argument, nullptr, 'h'},
        { nullptr, 0, nullptr, '\0' }};
    
    while ((option = getopt_long(argc, argv, "h", longOpts, &option_index))
           != -1) {
        switch (option) {
            case 'h':
                cout << "This program will read an input file of log entries\n"
                <<  "then enter an interactive mode where the user\n"
                <<  "can perform timestamp, category, and keyword searches\n"
                <<  "to construct an excerpt list.\n";
                exit(0);
        }
    }
}

/**
 *  utility function to convert a string to all lower case.
 *
 */
string str_tolower(string& s) {
    transform(s.begin(), s.end(), s.begin(),
              [](unsigned char c){ return tolower(c); }
              );
    return s;
}

/**
 * check to see whether a string is a valid time stamp.
 * it has to be in this format: 99:74:11:21:61
 */
bool is_valid_timestamp(const string& ts) {
    bool retval = true;
    
    if (ts.length() != 14) {
        return false;
    }
    
    for (size_t i = 0; i < 14; i++) {
        if ( i == 2 || i == 5 || i == 8 || i == 11) {
            retval = ( ts[i] == ':');
        } else {
            retval = isdigit(ts[i]);
        }
        
        if (!retval) {
            break;
        }
    }
    
    return retval;
}
/**
 *  ​mm:dd:hh:mm:ss
 */
int64_t convert_time_stamp(const string& ts) {
    return (int64_t)((ts[0]-'0')*pow(10,9) + (ts[1]-'0')*pow(10,8) + (ts[3]-'0')*pow(10,7) + (ts[4]-'0')*pow(10,6) + (ts[6]-'0')*pow(10,5) + (ts[7]-'0')*pow(10,4) +(ts[9]-'0')*pow(10,3) + (ts[10]-'0')*pow(10,2) + (ts[12]-'0')*10 + (ts[13]-'0'));
}

/**
 *  utility function to parse a string into a vector of words.
 *
 */
vector<string> parse_string(const string& input) {
    vector<string> retval;
    string kw = "";
    
    for (auto x : input) {
        // TODO: check to see whether # is a valid char in a keyword!!
        // if (isalnum(x) == 0 && x != '#') {
        if (isalnum(x) == 0) {
            if (kw.length() > 0) {
                retval.push_back(kw);
            }
            kw = "";
        } else {
            kw = kw + x;
        }
    }
    
    if (kw.length() > 0) {
        retval.push_back(kw);
    }
    
    return retval;
}

void add_to_keyword_map(string& kw, LogEntry* entry) {
    // make sure that the entry has not been added already
    if (keywordMap.count(str_tolower(kw)) == 0 ||
        keywordMap[str_tolower(kw)].back()->entryID != entry->entryID) {
        keywordMap[str_tolower(kw)].push_back(entry);
    }
}
/**
 *  parse the messge string into keywords, and add them to the hash map.
 *
 */
void process_message(const string& message, LogEntry* entry) {
    string kw = "";
    
    // cout << "Processing message: " << message << endl;
    
    for (auto x : message) {
        // TODO: check to see whether # is a valid char in a keyword!!
        // if (isalnum(x) == 0 && x != '#') {
        if (isalnum(x) == 0) {
            add_to_keyword_map(kw, entry);
            kw = "";
        } else {
            kw = kw + x;
        }
    }
    
    add_to_keyword_map(kw, entry);
}

/**
 *  read one line from log file and create one log entry.
 *
 */
void add_entry(const size_t& id, const string& line) {
    string timeField;
    string category;
    string message;
    
    // cout << "add_entry: " << line << endl;
    
    size_t firstBar = line.find('|');
    size_t secondBar = line.find('|', firstBar+1);
    
    timeField = line.substr(0, firstBar);
    category = line.substr(firstBar+1, secondBar - firstBar - 1);
    message = line.substr(secondBar + 1);
    
    LogEntry entry{id, convert_time_stamp(timeField), timeField, category, message};
    
    logEntries.push_back(entry);
    
}

/**
 *  Process the log entries:
 *  a). sort them based on timestamp;
 *  b). Store info in hash maps for searches
 *
 */
void process_log_entries() {
    
    // sort the logEntries vector
    sort(logEntries.begin(), logEntries.end(), CmpLogEntries());
    
    // after logEntries are sorted, then adding them to the categoryMap and keyword map
    // are in their sorted order, which is needed later.
    for(size_t i=0; i < logEntries.size(); i++) {
        string cat = logEntries[i].category;
        
        categoryMap[str_tolower(cat)].push_back(&logEntries[i]);
        
        // add category to the keyword map too since keyword search needs it
        process_message(cat, &logEntries[i]);
        
        // break message into individual keywords and add them to the hash map
        process_message(logEntries[i].message, &logEntries[i]);
        
    }
    
}
/**
 * Read the log file and store the entries.
 */
void read_logfile(string filename) {
    string line;
    ifstream logfile (filename);
    
    if (logfile.is_open()) {
        size_t id = 0;
        while ( getline (logfile, line) ) {
            add_entry(id, line);
            id++;
        }
        logfile.close();
    }
}

/**
 *  handle category search
 *
 */
void perform_category_search(string& cat) {
    
    // always clear the search results before performing category search
    searchResults.clear();
    
    // Note: category search does not need to parse words in the search string
    if (categoryMap.count(str_tolower(cat)) > 0) {
        searchResults = categoryMap[str_tolower(cat)];
    }
    
    cout << "Category search: " << searchResults.size() << " entries found\n";
}

/**
 *  perform keyword search
 *
 */
void perform_keyword_search(const string& argument) {
    
    // always clear the search results
    searchResults.clear();
    
    vector<string> keywords = parse_string(argument);
    
    bool firstTime = true;
    
    // go through the keyword map, and take the intersection of all keyword ID sets
    for (string kw: keywords) {
        if (keywordMap.count(str_tolower(kw)) > 0) {
            if (firstTime) {
                firstTime = false;
                searchResults.insert(searchResults.end(), keywordMap[str_tolower(kw)].begin(), keywordMap[str_tolower(kw)].end());
            } else {
                vector<LogEntry*> newEntries;
                
                set_intersection(searchResults.begin(), searchResults.end(),
                                 keywordMap[str_tolower(kw)].begin(),
                                 keywordMap[str_tolower(kw)].end(),
                                 inserter(newEntries, newEntries.begin()));
                
                searchResults = newEntries;
            }
        } else {
            // if there is no hit on any keyword, the intersection should be empty.
            // clear the potential elements, and break
            searchResults.clear();
            break;
        }
    }
    
    cout << "Keyword search: " << searchResults.size() << " entries found\n";
}

/**
 *  performing a binary search on the logEntries vector for the given entryID
 */
LogEntry* find_log_entry_by_id(const size_t& entryID) {
    // TODO: Is there a better way to find the entry without going through the
    // entire vector??
    for (size_t i = 0; i < logEntries.size(); i++) {
        if (logEntries[i].entryID == entryID) {
            return &logEntries[i];
        }
    }
    return nullptr;
}

/**
 * perform time search with a time range.
 *
 */
void perform_time_search(const string& argument) {
    if (argument.length() == 29) {
        string startTS = argument.substr(0, 14);
        string endTS = argument.substr(15, 14);
        if (is_valid_timestamp(startTS) && is_valid_timestamp(endTS)) {
            int64_t startTime = convert_time_stamp(startTS);
            int64_t endTime = convert_time_stamp(endTS);
            
            auto start = lower_bound(logEntries.begin(), logEntries.end(), startTime, CompTime());
            auto end = upper_bound(logEntries.begin(), logEntries.end(), endTime, CompTime());
            
            // only when the time stamps are valid, we would clear the search results
            searchResults.clear();
            
            // Add the pointers of the entries to the search results
            for (auto entry = start; entry != end; ++entry) {
                searchResults.push_back(&(*entry));
            }
            
            cout << "Timestamps search: " << searchResults.size() << " entries found\n";
        }
        
    }
}

/**
 *  perform time search by a given time stamp
 *
 */
void perform_time_match_search(const string& argument) {
    if (is_valid_timestamp(argument)) {
        // clear search results only when input is a valid timestamp
        searchResults.clear();
        
        pair<vector<LogEntry>::iterator, vector<LogEntry>::iterator> bounds;
        bounds = equal_range(logEntries.begin(), logEntries.end(), convert_time_stamp(argument), CompTime());
        
        for (auto entry = bounds.first; entry != bounds.second; ++entry) {
            searchResults.push_back(&(*entry));
        }
        
        cout << "Timestamp search: " << searchResults.size() << " entries found\n";
    }
}

/**
 *  Handle append a log entry by the given entry ID.
 *
 */
void append_log_entry(const string& argument) {
    // cout << "Performing appending log entry, entryID: " << argument << endl;
    size_t entryID = static_cast<size_t>(stoi(argument));
    LogEntry* entry = find_log_entry_by_id(entryID);
    
    if (entry != NULL) {
        excerptList.push_back(entry);
        cout << "log entry " << entryID << " appended\n";
    }
}

/**
 * delete an entry from the excerptList at a given position.
 */
void delete_log_entry(const string& argument) {
    size_t position = static_cast<size_t>(stoi(argument));
    
    if (position < excerptList.size()) {
        excerptList.erase(excerptList.begin()+(long)position);
        cout << "Deleted excerpt list entry " << position << "\n";
    }
}

/**
 *  move an entry to the front
 *
 */
void move_entry_to_front(const string& argument) {
    size_t position = static_cast<size_t>(stoi(argument));
    
    if (position < excerptList.size()) {
        excerptList.push_front(*(excerptList.begin()+(long)position));
        excerptList.erase(excerptList.begin()+(long)position+1);
        cout << "Moved excerpt list entry " << position << "\n";
    }
    
}

/**
 * move an entry to the back
 *
 */
void move_entry_to_back(const string& argument) {
    size_t position = static_cast<size_t>(stoi(argument));
    
    if (position < excerptList.size()) {
        excerptList.push_back(*(excerptList.begin()+(long)position));
        excerptList.erase(excerptList.begin()+(long)position);
        cout << "Moved excerpt list entry " << position << "\n";
    }
    
}


/**
 *  print what's in the excerptList.
 *
 */
void print_excerpt_list() {
    for (size_t i = 0; i < excerptList.size(); i++) {
        cout << i << '|' << excerptList[i]->entryID << '|' << excerptList[i]->timeField << '|' << excerptList[i]->category << '|' << excerptList[i]->message << "\n";
    }
}

/**
 *  print the list briefly
 *
 */
void print_excerpt_list_brief() {
    cout << "0|" << excerptList.front()->entryID << '|' << excerptList.front()->timeField << '|' << excerptList.front()->category << '|' << excerptList.front()->message << "\n";
    cout << "..." << "\n";
    
    if (excerptList.size() > 1) {
        cout << (excerptList.size()-1) << '|' << excerptList.back()->entryID << '|' << excerptList.back()->timeField << '|' << excerptList.back()->category << '|' << excerptList.back()->message << "\n";
    } else {
        cout << "0|" << excerptList.front()->entryID << '|' << excerptList.front()->timeField << '|' << excerptList.front()->category << '|' << excerptList.front()->message << "\n";
    }
}

/**
 *  clear exerpt list and print some info as required.
 *
 */
void delete_excerpt_list() {
    cout << "excerpt list cleared\n";
    
    if (!excerptList.empty()) {
        cout << "previous contents:\n";
        print_excerpt_list_brief();
        
        // Now clear the list
        excerptList.clear();
    } else {
        cout << "(previously empty)\n";
    }
    
}

/**
 *  sort the list by timestamp.
 *
 */
void sort_excerpt_list() {
    // sort the logEntries vector
    cout << "excerpt list sorted\n";
    
    if (excerptList.size() > 0) {
        
        cout << "previous ordering:\n";
        print_excerpt_list_brief();
        
        sort(excerptList.begin(), excerptList.end(), CmpLogEntriesPtrs());
        
        cout << "new ordering:\n";
        print_excerpt_list_brief();
        
    } else {
        cout << "(previously empty)\n";
    }
}

/**
 *  print the most recent search results
 *
 */
void print_search_results() {
    for (LogEntry* entry : searchResults) {
        cout << entry->entryID << '|' << entry->timeField << '|' << entry->category << '|' << entry->message << "\n";
    }
}

/**
 *  append most recent search results to the excerptList.
 */
void append_search_results() {
    if (searchResults.size() > 0) {
        excerptList.insert(excerptList.end(), searchResults.begin(), searchResults.end());
    }
    if (searchPerformed) {
        cout << searchResults.size() << " log entries appended\n";
    }
}

/**
 * handle one single prompt_command
 *
 */
void handle_command(const char& command, string& argument) {
    // cout << "handling command: " << command << ", argument: " << argument << endl;
    
    switch (command) {
        case 'a':
            append_log_entry(argument);
            break;
        case 'b':
            move_entry_to_front(argument);
            break;
        case 'c':
            perform_category_search(argument);
            searchPerformed = true;
            break;
        case 'd':
            delete_log_entry(argument);
            break;
        case 'e':
            move_entry_to_back(argument);
            break;
        case 'g':
            print_search_results();
            break;
        case 'k':
            perform_keyword_search(argument);
            searchPerformed = true;
            break;
        case 'l':
            delete_excerpt_list();
            break;
        case 'p':
            print_excerpt_list();
            break;
        case 'r':
            append_search_results();
            break;
        case 's':
            sort_excerpt_list();
            break;
        case 't':
            perform_time_search(argument);
            searchPerformed = true;
            break;
        case 'm':
            perform_time_match_search(argument);
            searchPerformed = true;
            break;
        default:
            break;
    }
}
/**
 *  handle one single command
 *
 */
void prompt_command() {
    string commandLine;
    char command = '.';
    string argument;
    
    cout << "% ";
    
    while (command != 'q') {
        
        getline(cin, commandLine);
        
        command = commandLine[0];
        
        // Skip comment lines
        if (command == '#') {
            cout << "% ";
            continue;
        } else if (command == 'q') {
            break;
        }
        
        if (commandLine.length() > 2) {
            argument = commandLine.substr(2);
        }
        else {
            argument = "";
        }
        
        handle_command(command, argument);
        
        cout << "% ";
    }
}

/**
 *  Main function.
 *
 */
int main(int argc, char** argv) {
    ios_base::sync_with_stdio(false);
    xcode_redirect(argc, argv);
    
    // get the options
    get_options(argc, argv);
    
    // read data from input file
    read_logfile(argv[1]);
    
    cout << logEntries.size() << " entries read\n";
    
    // process log entries to store contents in hash maps to support searches
    process_log_entries();
    
    // prompt command
    prompt_command();
    
    return 0;
}
