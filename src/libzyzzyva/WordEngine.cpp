//---------------------------------------------------------------------------
// WordEngine.cpp
//
// A class to handle the loading and searching of words.
//
// Copyright 2004, 2005, 2006 Michael W Thelen <mthelen@gmail.com>.
//
// This file is part of Zyzzyva.
//
// Zyzzyva is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Zyzzyva is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//---------------------------------------------------------------------------

#include "WordEngine.h"
#include "LetterBag.h"
#include "Auxil.h"
#include "Defs.h"
#include <QApplication>
#include <QFile>
#include <QRegExp>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <set>

const int MAX_DEFINITION_LINKS = 3;

using namespace Defs;
using std::set;
using std::map;
using std::make_pair;
using std::multimap;

//---------------------------------------------------------------------------
//  clearCache
//
//! Clear the word information cache.
//---------------------------------------------------------------------------
void
WordEngine::clearCache() const
{
    //qDebug ("Clearing the cache...");
    wordCache.clear();
}

//---------------------------------------------------------------------------
//  connectToDatabase
//
//! Initialize the database connection.
//
//! @param filename the name of the database file
//! @param errString returns the error string in case of error
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::connectToDatabase (const QString& filename, QString* errString)
{
    Rand rng;
    rng.srand (std::time (0), Auxil::getPid());
    unsigned int r = rng.rand();
    dbConnectionName = "WordEngine" + QString::number (r);

    db = QSqlDatabase::addDatabase ("QSQLITE", dbConnectionName);
    db.setDatabaseName (filename);
    bool ok = db.open();

    if (!ok) {
        dbConnectionName = QString();
        if (errString)
            *errString = db.lastError().text();
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------
//  disconnectFromDatabase
//
//! Remove the database connection.
//
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::disconnectFromDatabase()
{
    if (!db.isOpen() || dbConnectionName.isEmpty())
        return true;

    db.close();
    QSqlDatabase::removeDatabase (dbConnectionName);
    dbConnectionName = QString();
    return true;
}

//---------------------------------------------------------------------------
//  importTextFile
//
//! Import words from a text file.  The file is assumed to be in plain text
//! format, containing one word per line.
//
//! @param filename the name of the file to import
//! @param lexName the name of the lexicon
//! @param loadDefinitions whether to load word definitions
//! @param errString returns the error string in case of error
//! @return the number of words imported
//---------------------------------------------------------------------------
int
WordEngine::importTextFile (const QString& filename, const QString& lexName,
                            bool loadDefinitions, QString* errString)
{
    QFile file (filename);
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text)) {
        if (errString)
            *errString = "Can't open file '" + filename + "': "
            + file.errorString();
        return 0;
    }

    int imported = 0;
    char* buffer = new char [MAX_INPUT_LINE_LEN];
    while (file.readLine (buffer, MAX_INPUT_LINE_LEN) > 0) {
        QString line (buffer);
        line = line.simplified();
        if (!line.length() || (line.at (0) == '#'))
            continue;
        QString word = line.section (' ', 0, 0).toUpper();

        if (!graph.containsWord (word)) {
            QString alpha = Auxil::getAlphagram (word);
            ++numAnagramsMap[alpha];
        }

        graph.addWord (word);
        if (loadDefinitions) {
            QString definition = line.section (' ', 1);
            addDefinition (word, definition);
        }
        ++imported;
    }

    delete[] buffer;

    lexiconName = lexName;
    return imported;
}

//---------------------------------------------------------------------------
//  importDawgFile
//
//! Import words from a DAWG file as generated by Graham Toal's dawgutils
//! programs: http://www.gtoal.com/wordgames/dawgutils/
//
//! @param filename the name of the DAWG file to import
//! @param lexName the name of the lexicon
//! @param reverse whether the DAWG contains reversed words
//! @param errString returns the error string in case of error
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::importDawgFile (const QString& filename, const QString& lexName,
                            bool reverse, QString* errString, quint16*
                            expectedChecksum)
{
    bool ok = graph.importDawgFile (filename, reverse, errString,
                                    expectedChecksum);

    if (!ok)
        return false;

    if (!reverse)
        lexiconName = lexName;

    return true;
}

//---------------------------------------------------------------------------
//  importStems
//
//! Import stems from a file.  The file is assumed to be in plain text format,
//! containing one stem per line.  The file is also assumed to contain stems
//! of equal length.  All stems of different length than the first stem will
//! be discarded.
//
//! @param filename the name of the file to import
//! @param errString returns the error string in case of error
//! @return the number of stems imported
//---------------------------------------------------------------------------
int
WordEngine::importStems (const QString& filename, QString* errString)
{
    QFile file (filename);
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text)) {
        if (errString)
            *errString = "Can't open file '" + filename + "': "
            + file.errorString();
        return -1;
    }

    // XXX: At some point, may want to consider allowing words of varying
    // lengths to be in the same file?
    QStringList words;
    set<QString> alphagrams;
    int imported = 0;
    int length = 0;
    char* buffer = new char [MAX_INPUT_LINE_LEN];
    while (file.readLine (buffer, MAX_INPUT_LINE_LEN) > 0) {
        QString line (buffer);
        line = line.simplified();
        if (!line.length() || (line.at (0) == '#'))
            continue;
        QString word = line.section (' ', 0, 0);

        if (!length)
            length = word.length();

        if (length != int (word.length()))
            continue;

        words << word;
        alphagrams.insert (Auxil::getAlphagram (word));
        ++imported;
    }
    delete[] buffer;

    // Insert the stem list into the map, or append to an existing stem list
    map<int, QStringList>::iterator it = stems.find (length);
    if (it == stems.end()) {
        stems.insert (make_pair (length, words));
        stemAlphagrams.insert (make_pair (length, alphagrams));
    }
    else {
        it->second += words;
        std::map< int, set<QString> >::iterator it =
            stemAlphagrams.find (length);
        set<QString>::iterator sit;
        for (sit = alphagrams.begin(); sit != alphagrams.end(); ++sit)
            (it->second).insert (*sit);
    }

    return imported;
}

//---------------------------------------------------------------------------
//  getNewInOwl2String
//
//! Read all new OWL2 words into a string, separated by spaces.  XXX: Right
//! now this is hard-coded to load a certain file for a specific purpose.
//! This whole concept should be more flexible.
//---------------------------------------------------------------------------
QString
WordEngine::getNewInOwl2String() const
{
    QFile file (Auxil::getWordsDir() + "/north-american/owl2-new-words.txt");
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
        return QString::null;

    QStringList words;
    char* buffer = new char [MAX_INPUT_LINE_LEN];
    while (file.readLine (buffer, MAX_INPUT_LINE_LEN) > 0) {
        QString line (buffer);
        line = line.simplified();
        if (!line.length() || (line.at (0) == '#'))
            continue;
        QString word = line.section (' ', 0, 0);
        words.append (word);
    }
    delete[] buffer;

    return words.join (" ");
}

//---------------------------------------------------------------------------
//  databaseSearch
//
//! Search the database for words matching the conditions in a search spec.
//! If a word list is provided, also ensure that result words are in that
//! list.
//
//! @param optimizedSpec the search spec
//! @param wordList optional list of words that results must be in
//! @return a list of words matching the search spec
//---------------------------------------------------------------------------
QStringList
WordEngine::databaseSearch (const SearchSpec& optimizedSpec,
                            const QStringList* wordList) const
{
    // Build SQL query string
    QString queryStr = "SELECT word FROM words WHERE";
    bool foundCondition = false;
    QListIterator<SearchCondition> cit (optimizedSpec.conditions);
    while (cit.hasNext()) {
        SearchCondition condition = cit.next();
        switch (condition.type) {
            case SearchCondition::Length:
            case SearchCondition::InWordList:
            case SearchCondition::NumVowels:
            case SearchCondition::IncludeLetters:
            case SearchCondition::ProbabilityOrder:
            case SearchCondition::NumUniqueLetters:
            case SearchCondition::PointValue:
            case SearchCondition::NumAnagrams:
            if (foundCondition)
                queryStr += " AND";
            foundCondition = true;
            break;

            default:
            break;
        }

        switch (condition.type) {
            case SearchCondition::ProbabilityOrder: {
                // Lax boundaries
                if (condition.boolValue) {
                    queryStr += " max_probability_order>=" +
                        QString::number (condition.minValue)
                        + " AND min_probability_order<=" +
                        QString::number (condition.maxValue);
                }
                // Strict boundaries
                else {
                    queryStr += " probability_order";
                    if (condition.minValue == condition.maxValue) {
                        queryStr += "=" + QString::number (condition.minValue);
                    }
                    else {
                        queryStr += ">=" + QString::number (condition.minValue)
                            + " AND probability_order<=" +
                            QString::number (condition.maxValue);
                    }
                }
            }
            break;

            case SearchCondition::Length:
            case SearchCondition::NumVowels:
            case SearchCondition::NumUniqueLetters:
            case SearchCondition::PointValue:
            case SearchCondition::NumAnagrams: {
                QString column;
                if (condition.type == SearchCondition::Length)
                    column = "length";
                if (condition.type == SearchCondition::NumVowels)
                    column = "num_vowels";
                if (condition.type == SearchCondition::NumUniqueLetters)
                    column = "num_unique_letters";
                if (condition.type == SearchCondition::PointValue)
                    column = "point_value";
                if (condition.type == SearchCondition::NumAnagrams)
                    column = "num_anagrams";

                queryStr += " " + column;
                if (condition.minValue == condition.maxValue) {
                    queryStr += "=" + QString::number (condition.minValue);
                }
                else {
                    queryStr += ">=" + QString::number (condition.minValue) +
                        " AND " + column + "<=" +
                        QString::number (condition.maxValue);
                }
            }
            break;

            case SearchCondition::IncludeLetters: {
                QString str = condition.stringValue;
                for (int i = 0; i < str.length(); ++i) {
                    QChar c = str.at (i);
                    if (i)
                        queryStr += " AND";
                    queryStr += " word";
                    if (condition.negated)
                        queryStr += " NOT";
                    queryStr += " LIKE '%" + QString (c) + "%'";
                }
            }
            break;

            case SearchCondition::InWordList: {
                queryStr += " word";
                if (condition.negated)
                    queryStr += " NOT";
                queryStr += " IN (";
                QStringList words = condition.stringValue.split (QChar (' '));
                QStringListIterator it (words);
                bool firstWord = true;
                while (it.hasNext()) {
                    QString word = it.next();
                    if (!firstWord)
                        queryStr += ",";
                    firstWord = false;
                    queryStr += "'" + word + "'";
                }
                queryStr += ")";
            }
            break;

            default:
            break;
        }

    }

    // Make sure results are in the provided word list
    QMap<QString, QString> upperToLower;
    if (wordList) {
        queryStr += " AND word IN (";
        QStringListIterator it (*wordList);
        bool firstWord = true;
        while (it.hasNext()) {
            QString word = it.next();
            QString wordUpper = word.toUpper();
            upperToLower[wordUpper] = word;
            if (!firstWord)
                queryStr += ",";
            firstWord = false;
            queryStr += "'" + wordUpper + "'";
        }
        queryStr += ")";
    }

    // Query the database
    QStringList resultList;
    QSqlQuery query (queryStr, db);
    while (query.next()) {
        QString word = query.value (0).toString();
        if (!upperToLower.isEmpty() && upperToLower.contains (word)) {
            word = upperToLower[word];
        }
        resultList.append (word);
    }

    return resultList;
}

//---------------------------------------------------------------------------
//  applyPostConditions
//
//! Limit search results by search conditions that cannot be easily used to
//! limit word graph or database searches.
//
//! @param optimizedSpec the search spec
//! @param wordList optional list of words that results must be in
//! @return a list of words matching the search spec
//---------------------------------------------------------------------------
QStringList
WordEngine::applyPostConditions (const SearchSpec& optimizedSpec, const
                                 QStringList& wordList) const
{
    QStringList returnList = wordList;

    // Check special postconditions
    QStringList::iterator wit;
    for (wit = returnList.begin(); wit != returnList.end();) {
        if (matchesConditions (*wit, optimizedSpec.conditions))
            ++wit;
        else
            wit = returnList.erase (wit);
    }

    // Handle Limit by Probability Order conditions
    bool probLimitRangeCondition = false;
    bool legacyProbCondition = false;
    int probLimitRangeMin = 0;
    int probLimitRangeMax = 999999;
    int probLimitRangeMinLax = 0;
    int probLimitRangeMaxLax = 999999;
    QListIterator<SearchCondition> cit (optimizedSpec.conditions);
    while (cit.hasNext()) {
        SearchCondition condition = cit.next();
        if (condition.type == SearchCondition::LimitByProbabilityOrder) {
            probLimitRangeCondition = true;
            if (condition.boolValue) {
                if (condition.minValue > probLimitRangeMinLax)
                    probLimitRangeMinLax = condition.minValue;
                if (condition.maxValue < probLimitRangeMaxLax)
                    probLimitRangeMaxLax = condition.maxValue;
            }
            else {
                if (condition.minValue > probLimitRangeMin)
                    probLimitRangeMin = condition.minValue;
                if (condition.maxValue < probLimitRangeMax)
                    probLimitRangeMax = condition.maxValue;
            }
            if (condition.legacy)
                legacyProbCondition = true;
        }
    }

    // Keep only words in the probability order range
    if (probLimitRangeCondition) {

        if ((probLimitRangeMin > returnList.size()) ||
            (probLimitRangeMinLax > returnList.size()))
        {
            returnList.clear();
            return returnList;
        }

        // Convert from 1-based to 0-based offset
        --probLimitRangeMin;
        --probLimitRangeMax;
        --probLimitRangeMinLax;
        --probLimitRangeMaxLax;

        if (probLimitRangeMin < 0)
            probLimitRangeMin = 0;
        if (probLimitRangeMinLax < 0)
            probLimitRangeMinLax = 0;
        if (probLimitRangeMax > returnList.size() - 1)
            probLimitRangeMax = returnList.size() - 1;
        if (probLimitRangeMaxLax > returnList.size() - 1)
            probLimitRangeMaxLax = returnList.size() - 1;

        // Use the higher of the min values as working min
        int min = ((probLimitRangeMin > probLimitRangeMinLax)
                   ? probLimitRangeMin : probLimitRangeMinLax);

        // Use the lower of the max values as working max
        int max = ((probLimitRangeMax < probLimitRangeMaxLax)
                   ? probLimitRangeMax : probLimitRangeMaxLax);

        // Sort the words according to probability order
        LetterBag bag;
        QMap<QString, QString> probMap;

        QString word;
        foreach (word, returnList) {
            // FIXME: change this radix for new probability sorting - leave
            // alone for old probability sorting
            QString radix;
            QString wordUpper = word.toUpper();
            radix.sprintf ("%09.0f",
                1e9 - 1 - bag.getNumCombinations (wordUpper));
            // Legacy probability order limits are sorted alphabetically, not
            // by alphagram
            if (!legacyProbCondition)
                radix += Auxil::getAlphagram (wordUpper);
            radix += wordUpper;
            probMap.insert (radix, word);
        }

        QStringList keys = probMap.keys();

        QString minRadix = keys[min];
        QString minCombinations = minRadix.left (9);
        while ((min > 0) && (min > probLimitRangeMin)) {
            if (minCombinations != keys[min - 1].left (9))
                break;
            --min;
        }

        QString maxRadix = keys[max];
        QString maxCombinations = maxRadix.left (9);
        while ((max < keys.size() - 1) && (max < probLimitRangeMax)) {
            if (maxCombinations != keys[max + 1].left (9))
                break;
            ++max;
        }

        returnList = probMap.values().mid (min, max - min + 1);
    }

    return returnList;
}

//---------------------------------------------------------------------------
//  isAcceptable
//
//! Determine whether a word is acceptable.
//
//! @param word the word to look up
//! @return true if acceptable, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::isAcceptable (const QString& word) const
{
    return graph.containsWord (word);
}

//---------------------------------------------------------------------------
//  search
//
//! Search for acceptable words matching a search specification.
//
//! @param spec the search specification
//! @param allCaps whether to ensure the words in the list are all caps
//! @return a list of acceptable words
//---------------------------------------------------------------------------
QStringList
WordEngine::search (const SearchSpec& spec, bool allCaps) const
{
    SearchSpec optimizedSpec = spec;
    optimizedSpec.optimize();

    // Discover which kinds of search conditions are present
    int postConditions = 0;
    int wordGraphConditions = 0;
    int databaseConditions = 0;
    int wildcardConditions = 0;
    int nonWildcardConditions = 0;
    int lengthConditions = 0;
    QListIterator<SearchCondition> cit (optimizedSpec.conditions);
    while (cit.hasNext()) {
        SearchCondition condition = cit.next();
        switch (condition.type) {
            case SearchCondition::BelongToGroup:
            case SearchCondition::Prefix:
            case SearchCondition::Suffix:
            case SearchCondition::LimitByProbabilityOrder:
            ++postConditions;
            break;

            case SearchCondition::AnagramMatch:
            case SearchCondition::PatternMatch:
            if (condition.stringValue.contains ("*"))
                ++wildcardConditions;
            else
                ++nonWildcardConditions;
            ++wordGraphConditions;
            break;

            case SearchCondition::SubanagramMatch:
            case SearchCondition::ConsistOf:
            ++wordGraphConditions;
            break;

            case SearchCondition::Length:
            ++lengthConditions;
            ++databaseConditions;
            break;

            case SearchCondition::InWordList:
            case SearchCondition::NumVowels:
            case SearchCondition::IncludeLetters:
            case SearchCondition::ProbabilityOrder:
            case SearchCondition::NumUniqueLetters:
            case SearchCondition::PointValue:
            case SearchCondition::NumAnagrams:
            ++databaseConditions;
            break;

            default:
            break;
        }
    }

    // Do not search the database based on Length conditions that were only
    // added by SearchSpec::optimize to optimize word graph searches
    if ((wordGraphConditions) &&
        (databaseConditions >= 1) && (lengthConditions == databaseConditions))
    {
        --databaseConditions;
    }

    // Search the word graph if necessary
    QStringList resultList;
    if (wordGraphConditions || !databaseConditions) {
        resultList = wordGraphSearch (optimizedSpec);
        if (resultList.isEmpty())
            return resultList;
    }

    // Search the database if necessary, passing word graph results
    if (databaseConditions) {
        resultList = databaseSearch (optimizedSpec,
                                     wordGraphConditions ? &resultList : 0);
        if (resultList.isEmpty())
            return resultList;
    }

    // Check post conditions if necessary
    if (postConditions) {
        resultList = applyPostConditions (optimizedSpec, resultList);
    }

    // Convert to all caps if necessary
    if (allCaps) {
        QStringList::iterator it;
        for (it = resultList.begin(); it != resultList.end(); ++it)
            *it = (*it).toUpper();
    }

    if (!resultList.isEmpty()) {
        clearCache();
        addToCache (resultList);
    }

    return resultList;
}

//---------------------------------------------------------------------------
//  wordGraphSearch
//
//! Search the word graph for words matching the conditions in a search spec.
//
//! @param optimizedSpec the search spec
//! @return a list of words
//---------------------------------------------------------------------------
QStringList
WordEngine::wordGraphSearch (const SearchSpec& optimizedSpec) const
{
    return graph.search (optimizedSpec);
}

//---------------------------------------------------------------------------
//  alphagrams
//
//! Transform a list of strings into a list of alphagrams of those strings.
//! The created list may be shorter than the original list.
//
//! @param list the list of strings
//! @return a list of alphagrams
//---------------------------------------------------------------------------
QStringList
WordEngine::alphagrams (const QStringList& list) const
{
    QStringList alphagrams;
    QStringList::const_iterator it;

    // Insert into a set first, to remove duplicates
    set<QString> seen;
    for (it = list.begin(); it != list.end(); ++it) {
        seen.insert (Auxil::getAlphagram (*it));
    }

    set<QString>::iterator sit;
    for (sit = seen.begin(); sit != seen.end(); ++sit) {
        alphagrams << *sit;
    }
    return alphagrams;
}

//---------------------------------------------------------------------------
//  getWordInfo
//
//! Get information about a word from the database.  Also cache the
//! information for future queries.  Fail if the information is not in the
//! cache and the database is not open.
//
//! @param word the word
//! @return information about the word from the database
//---------------------------------------------------------------------------
WordEngine::WordInfo
WordEngine::getWordInfo (const QString& word) const
{
    if (word.isEmpty())
        return WordInfo();

    if (wordCache.contains (word)) {
        //qDebug ("Cache HIT: |%s|", word.toUtf8().data());
        return wordCache[word];
    }
    //qDebug ("Cache MISS: |%s|", word.toUtf8().data());

    WordInfo info;
    if (!db.isOpen())
        return info;

    QString qstr = "SELECT probability_order, min_probability_order, "
        "max_probability_order, num_vowels, num_unique_letters, num_anagrams, "
        "point_value, front_hooks, back_hooks, definition FROM words "
        "WHERE word=?";
    QSqlQuery query (db);
    query.prepare (qstr);
    query.bindValue (0, word);
    query.exec();
 
    if (query.next()) {
        info.word = word;
        info.probabilityOrder    = query.value (0).toInt();
        info.minProbabilityOrder = query.value (1).toInt();
        info.maxProbabilityOrder = query.value (2).toInt();
        info.numVowels           = query.value (3).toInt();
        info.numUniqueLetters    = query.value (4).toInt();
        info.numAnagrams         = query.value (5).toInt();
        info.pointValue          = query.value (6).toInt();
        info.frontHooks          = query.value (7).toString();
        info.backHooks           = query.value (8).toString();
        info.definition          = query.value (9).toString();
        wordCache[word] = info;
    }

    return info;
}

//---------------------------------------------------------------------------
//  getNumWords
//
//! Return a word count for the current lexicon.
//
//! @return the word count
//---------------------------------------------------------------------------
int
WordEngine::getNumWords() const
{
    if (db.isOpen()) {
        QString qstr = "SELECT count(*) FROM words";
        QSqlQuery query (qstr, db);
        if (query.next())
            return query.value (0).toInt();
    }
    else
        return graph.getNumWords();
    return 0;
}

//---------------------------------------------------------------------------
//  getDefinition
//
//! Return the definition associated with a word.
//
//! @param word the word whose definition to look up
//! @param replaceLinks whether to resolve links to other definitions
//! @return the definition, or QString::null if no definition
//---------------------------------------------------------------------------
QString
WordEngine::getDefinition (const QString& word, bool replaceLinks) const
{
    QString definition;

    WordInfo info = getWordInfo (word);
    if (info.isValid()) {
        if (replaceLinks) {
            QStringList defs = info.definition.split (" / ");
            definition = "";
            QString def;
            foreach (def, defs) {
                if (!definition.isEmpty())
                    definition += "\n";
                //definition += def;
                definition += replaceDefinitionLinks (def,
                                                      MAX_DEFINITION_LINKS);
            }
            return definition;
        }
        else {
            return info.definition;
        }
    }

    else {
        map<QString, multimap<QString, QString> >::const_iterator it =
            definitions.find (word);
        if (it == definitions.end())
            return QString::null;

        const multimap<QString, QString>& mmap = it->second;
        multimap<QString, QString>::const_iterator mit;
        for (mit = mmap.begin(); mit != mmap.end(); ++mit) {
            if (!definition.isEmpty()) {
                if (replaceLinks)
                    definition += "\n";
                else
                    definition += " / ";
            }
            //definition += mit->second;
            definition += replaceLinks
                ? replaceDefinitionLinks (mit->second, MAX_DEFINITION_LINKS)
                : mit->second;
        }
        return definition;
    }
}

//---------------------------------------------------------------------------
//  getFrontHookLetters
//
//! Get a string of letters that can be added to the front of a word to make
//! other valid words.
//
//! @param word the word, assumed to be upper case
//! @return a string containing lower case letters representing front hooks
//---------------------------------------------------------------------------
QString
WordEngine::getFrontHookLetters (const QString& word) const
{
    QString ret;

    WordInfo info = getWordInfo (word);
    if (info.isValid()) {
        ret = info.frontHooks;
    }

    else {
        SearchSpec spec;
        SearchCondition condition;
        condition.type = SearchCondition::PatternMatch;
        condition.stringValue = "?" + word;
        spec.conditions.append (condition);

        // Put first letter of each word in a set, for alphabetical order
        QStringList words = search (spec, true);
        set<QChar> letters;
        QStringList::iterator it;
        for (it = words.begin(); it != words.end(); ++it)
            letters.insert ((*it).at (0).toLower());

        set<QChar>::iterator sit;
        for (sit = letters.begin(); sit != letters.end(); ++sit)
            ret += *sit;
    }

    return ret;
}

//---------------------------------------------------------------------------
//  getBackHookLetters
//
//! Get a string of letters that can be added to the back of a word to make
//! other valid words.
//
//! @param word the word, assumed to be upper case
//! @return a string containing lower case letters representing back hooks
//---------------------------------------------------------------------------
QString
WordEngine::getBackHookLetters (const QString& word) const
{
    QString ret;

    WordInfo info = getWordInfo (word);
    if (info.isValid()) {
        ret = info.backHooks;
    }

    else {
        SearchSpec spec;
        SearchCondition condition;
        condition.type = SearchCondition::PatternMatch;
        condition.stringValue = word + "?";
        spec.conditions.append (condition);

        // Put first letter of each word in a set, for alphabetical order
        QStringList words = search (spec, true);
        set<QChar> letters;
        QStringList::iterator it;
        for (it = words.begin(); it != words.end(); ++it)
            letters.insert ((*it).at ((*it).length() - 1).toLower());

        set<QChar>::iterator sit;
        for (sit = letters.begin(); sit != letters.end(); ++sit)
            ret += *sit;
    }

    return ret;
}

//---------------------------------------------------------------------------
//  addToCache
//
//! Add information about a list of words to the cache.
//
//! @param words the list of words
//---------------------------------------------------------------------------
void
WordEngine::addToCache (const QStringList& words) const
{
    if (words.isEmpty() || !db.isOpen())
        return;

    QString qstr = "SELECT word, probability_order, min_probability_order, "
        "max_probability_order, num_vowels, num_unique_letters, num_anagrams, "
        "point_value, front_hooks, back_hooks, definition FROM words "
        "WHERE word IN (";

    QStringListIterator it (words);
    for (int i = 0; it.hasNext(); ++i) {
        if (i)
            qstr += ", ";
        qstr += "'" + it.next() + "'";
    }
    qstr += ")";

    QSqlQuery query (db);
    query.prepare (qstr);
    query.exec();

    while (query.next()) {
        WordInfo info;
        info.word                = query.value (0).toString();
        info.probabilityOrder    = query.value (1).toInt();
        info.minProbabilityOrder = query.value (2).toInt();
        info.maxProbabilityOrder = query.value (3).toInt();
        info.numVowels           = query.value (4).toInt();
        info.numUniqueLetters    = query.value (5).toInt();
        info.numAnagrams         = query.value (6).toInt();
        info.pointValue          = query.value (7).toInt();
        info.frontHooks          = query.value (8).toString();
        info.backHooks           = query.value (9).toString();
        info.definition          = query.value (10).toString();
        wordCache[info.word] = info;
    }
}

//---------------------------------------------------------------------------
//  matchesConditions
//
//! Test whether a word matches certain conditions.  Not all conditions in the
//! list are tested.  Only the conditions that cannot be easily tested in
//! WordGraph::search are tested here.
//
//! @param word the word to be tested
//! @param conditions the list of conditions to test
//! @return true if the word matches all special conditions, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::matchesConditions (const QString& word, const
                               QList<SearchCondition>& conditions) const
{
    // FIXME: For conditions that can be tested by querying the database, a
    // query should be constructed that tests all the conditions as part of a
    // single WHERE clause.  This should be much more efficient than testing
    // each condition on each word individually, which requires several
    // queries.

    QString wordUpper = word.toUpper();
    QListIterator<SearchCondition> it (conditions);
    while (it.hasNext()) {
        const SearchCondition& condition = it.next();

        switch (condition.type) {

            case SearchCondition::Prefix:
            if ((!isAcceptable (condition.stringValue + wordUpper))
                ^ condition.negated)
                return false;
            break;

            case SearchCondition::Suffix:
            if ((!isAcceptable (wordUpper + condition.stringValue))
                ^ condition.negated)
                return false;
            break;

            case SearchCondition::BelongToGroup: {
                SearchSet searchSet = Auxil::stringToSearchSet
                    (condition.stringValue);
                if (searchSet == UnknownSearchSet)
                    continue;
                if (!isSetMember (wordUpper, searchSet) ^ condition.negated)
                    return false;
            }
            break;

            default: break;
        }
    }

    return true;
}

//---------------------------------------------------------------------------
//  isSetMember
//
//! Determine whether a word is a member of a set.  Assumes the word has
//! already been determined to be acceptable.
//
//! @param word the word to look up
//! @param ss the search set
//! @return true if a member of the set, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::isSetMember (const QString& word, SearchSet ss) const
{
    static QString typeTwoChars = "AAADEEEEGIIILNNOORRSSTTU";
    static int typeTwoCharsLen = typeTwoChars.length();
    static LetterBag letterBag ("A:9 B:2 C:2 D:4 E:12 F:2 G:3 H:2 I:9 J:1 "
                                "K:1 L:4 M:2 N:6 O:8 P:2 Q:1 R:6 S:4 T:6 "
                                "U:4 V:2 W:2 X:1 Y:2 Z:1 _:2");
    static double typeThreeSevenCombos
        = letterBag.getNumCombinations ("HUNTERS");
    static double typeThreeEightCombos
        = letterBag.getNumCombinations ("NOTIFIED");

    switch (ss) {
        case SetHookWords:
        return (isAcceptable (word.left (word.length() - 1)) ||
                isAcceptable (word.right (word.length() - 1)));

        case SetFrontHooks:
        return isAcceptable (word.right (word.length() - 1));

        case SetBackHooks:
        return isAcceptable (word.left (word.length() - 1));

        case SetTypeOneSevens: {
            if (word.length() != 7)
                return false;

            std::map< int, set<QString> >::const_iterator it =
                stemAlphagrams.find (word.length() - 1);
            if (it == stemAlphagrams.end())
                return false;

            const set<QString>& alphaset = it->second;
            QString agram = Auxil::getAlphagram (word);
            set<QString>::const_iterator ait;
            for (int i = 0; i < int (agram.length()); ++i) {
                ait = alphaset.find (agram.left (i) +
                                     agram.right (agram.length() - i - 1));
                if (ait != alphaset.end())
                    return true;
            }
            return false;
        }

        case SetTypeOneEights: {
            if (word.length() != 8)
                return false;

            std::map< int, set<QString> >::const_iterator it =
                stemAlphagrams.find (word.length() - 2);
            if (it == stemAlphagrams.end())
                return false;

            QString agram = Auxil::getAlphagram (word);

            // Compare the letters of the word with the letters of each
            // alphagram, ensuring that no more than two letters in the word
            // are missing from the alphagram.
            const set<QString>& alphaset = it->second;
            set<QString>::const_iterator ait;
            for (ait = alphaset.begin(); ait != alphaset.end(); ++ait) {
                QString setAlphagram = *ait;
                int missing = 0;
                int saIndex = 0;
                for (int i = 0; (i < int (agram.length())) &&
                                (saIndex < setAlphagram.length()); ++i)
                {
                    if (agram.at (i) == setAlphagram.at (saIndex))
                        ++saIndex;
                    else
                        ++missing;
                    if (missing > 2)
                        break;
                }
                if (missing <= 2)
                    return true;
            }
            return false;
        }

        case SetTypeTwoSevens:
        case SetTypeTwoEights:
        {
            if (((ss == SetTypeTwoSevens) && (word.length() != 7)) ||
                ((ss == SetTypeTwoEights) && (word.length() != 8)))
                return false;

            bool ok = false;
            QString alphagram = Auxil::getAlphagram (word);
            int wi = 0;
            QChar wc = alphagram[wi];
            for (int ti = 0; ti < typeTwoCharsLen; ++ti) {
                QChar tc = typeTwoChars[ti];
                if (tc == wc) {
                    ++wi;
                    if (wi == alphagram.length()) {
                        ok = true;
                        break;
                    }
                    wc = alphagram[wi];
                }
            }
            return (ok && !isSetMember (word, (ss == SetTypeTwoSevens ?
                                               SetTypeOneSevens :
                                               SetTypeOneEights)));
        }

        case SetTypeThreeSevens: {
            if (word.length() != 7)
                return false;

            double combos = letterBag.getNumCombinations (word);
            return ((combos >= typeThreeSevenCombos) &&
                    !isSetMember (word, SetTypeOneSevens) &&
                    !isSetMember (word, SetTypeTwoSevens));
        }

        case SetTypeThreeEights: {
            if (word.length() != 8)
                return false;

            double combos = letterBag.getNumCombinations (word);
            return ((combos >= typeThreeEightCombos) &&
                    !isSetMember (word, SetTypeOneEights) &&
                    !isSetMember (word, SetTypeTwoEights));
        }

        case SetEightsFromSevenLetterStems: {
            if (word.length() != 8)
                return false;

            std::map< int, set<QString> >::const_iterator it =
                stemAlphagrams.find (word.length() - 1);
            if (it == stemAlphagrams.end())
                return false;

            const set<QString>& alphaset = it->second;
            QString agram = Auxil::getAlphagram (word);
            set<QString>::const_iterator ait;
            for (int i = 0; i < int (agram.length()); ++i) {
                ait = alphaset.find (agram.left (i) +
                                     agram.right (agram.length() - i - 1));
                if (ait != alphaset.end())
                    return true;
            }
            return false;
        }


        default: return false;
    }
}

//---------------------------------------------------------------------------
//  getNumAnagrams
//
//! Determine the number of valid anagrams of a word.
//
//! @param word the word
//! @return the number of valid anagrams
//---------------------------------------------------------------------------
int
WordEngine::getNumAnagrams (const QString& word) const
{
    WordInfo info = getWordInfo (word);
    if (info.isValid()) {
        return info.numAnagrams;
    }

    else {
        QString alpha = Auxil::getAlphagram (word);
        return numAnagramsMap.contains (alpha) ? numAnagramsMap[alpha] : 0;
    }
}

//---------------------------------------------------------------------------
//  getProbabilityOrder
//
//! Get the probability order for a word.
//
//! @param word the word
//! @return the probability order
//---------------------------------------------------------------------------
int
WordEngine::getProbabilityOrder (const QString& word) const
{
    WordInfo info = getWordInfo (word);
    return info.isValid() ? info.probabilityOrder : 0;
}

//---------------------------------------------------------------------------
//  getMinProbabilityOrder
//
//! Get the minimum probability order for a word.
//
//! @param word the word
//! @return the probability order
//---------------------------------------------------------------------------
int
WordEngine::getMinProbabilityOrder (const QString& word) const
{
    WordInfo info = getWordInfo (word);
    return info.isValid() ? info.minProbabilityOrder : 0;
}

//---------------------------------------------------------------------------
//  getMaxProbabilityOrder
//
//! Get the maximum probability order for a word.
//
//! @param word the word
//! @return the probability order
//---------------------------------------------------------------------------
int
WordEngine::getMaxProbabilityOrder (const QString& word) const
{
    WordInfo info = getWordInfo (word);
    return info.isValid() ? info.maxProbabilityOrder : 0;
}

//---------------------------------------------------------------------------
//  getNumVowels
//
//! Get the number of vowels in a word.
//
//! @param word the word
//! @return the number of vowels
//---------------------------------------------------------------------------
int
WordEngine::getNumVowels (const QString& word) const
{
    WordInfo info = getWordInfo (word);
    return info.isValid() ? info.numVowels : Auxil::getNumVowels (word);
}

//---------------------------------------------------------------------------
//  getNumUniqueLetters
//
//! Get the number of unique letters in a word.
//
//! @param word the word
//! @return the number of unique letters
//---------------------------------------------------------------------------
int
WordEngine::getNumUniqueLetters (const QString& word) const
{
    WordInfo info = getWordInfo (word);
    return info.isValid() ? info.numUniqueLetters
                          : Auxil::getNumUniqueLetters (word);
}

//---------------------------------------------------------------------------
//  getPointValue
//
//! Get the point value for a word.
//
//! @param word the word
//! @return the point value
//---------------------------------------------------------------------------
int
WordEngine::getPointValue (const QString& word) const
{
    WordInfo info = getWordInfo (word);
    return info.isValid() ? info.pointValue : 0;
}

//---------------------------------------------------------------------------
//  nonGraphSearch
//
//! Search for valid words matching conditions that can be matched without
//! searching the word graph.
//
//! @param spec the search specification
//
//! @return a list of acceptable words matching the In Word List conditions
//---------------------------------------------------------------------------
QStringList
WordEngine::nonGraphSearch (const SearchSpec& spec) const
{
    QStringList wordList;
    set<QString> finalWordSet;
    set<QString>::iterator sit;
    int conditionNum = 0;

    const int MAX_ANAGRAMS = 65535;
    int minAnagrams = 0;
    int maxAnagrams = MAX_ANAGRAMS;
    int minNumVowels = 0;
    int maxNumVowels = MAX_WORD_LEN;
    int minNumUniqueLetters = 0;
    int maxNumUniqueLetters = MAX_WORD_LEN;
    int minPointValue = 0;
    int maxPointValue = 10 * MAX_WORD_LEN;

    // Look for InWordList conditions first, to narrow the search as much as
    // possible
    QListIterator<SearchCondition> it (spec.conditions);
    while (it.hasNext()) {
        const SearchCondition& condition = it.next();

        // Note the minimum and maximum number of anagrams from any Number of
        // Anagrams conditions
        if (condition.type == SearchCondition::NumAnagrams) {
            if ((condition.minValue > maxAnagrams) ||
                (condition.maxValue < minAnagrams))
                return wordList;
            if (condition.minValue > minAnagrams)
                minAnagrams = condition.minValue;
            if (condition.maxValue < maxAnagrams)
                maxAnagrams = condition.maxValue;
        }

        // Note the minimum and maximum number of vowels from any Number of
        // Vowels conditions
        else if (condition.type == SearchCondition::NumVowels) {
            if ((condition.minValue > maxNumVowels) ||
                (condition.maxValue < minNumVowels))
                return wordList;
            if (condition.minValue > minNumVowels)
                minNumVowels = condition.minValue;
            if (condition.maxValue < maxNumVowels)
                maxNumVowels = condition.maxValue;
        }

        // Note the minimum and maximum number of unique letters from any
        // Number of Unique Letters conditions
        else if (condition.type == SearchCondition::NumUniqueLetters) {
            if ((condition.minValue > maxNumUniqueLetters) ||
                (condition.maxValue < minNumUniqueLetters))
                return wordList;
            if (condition.minValue > minNumUniqueLetters)
                minNumUniqueLetters = condition.minValue;
            if (condition.maxValue < maxNumUniqueLetters)
                maxNumUniqueLetters = condition.maxValue;
        }

        // Note the minimum and maximum point value from any Point Value
        // conditions
        else if (condition.type == SearchCondition::PointValue) {
            if ((condition.minValue > maxPointValue) ||
                (condition.maxValue < minPointValue))
                return wordList;
            if (condition.minValue > minPointValue)
                minPointValue = condition.minValue;
            if (condition.maxValue < maxPointValue)
                maxPointValue = condition.maxValue;
        }

        // Only InWordList conditions allowed beyond this point - look up
        // words for acceptability and combine the word lists
        if (condition.type != SearchCondition::InWordList)
            continue;
        QStringList words = condition.stringValue.split (QChar (' '));

        set<QString> wordSet;
        QStringList::iterator wit;
        for (wit = words.begin(); wit != words.end(); ++wit) {
            if (isAcceptable (*wit))
                wordSet.insert (*wit);
        }

        // Combine search result set with words already found
        if (!conditionNum) {
            finalWordSet = wordSet;
        }

        else if (spec.conjunction) {
            set<QString> conjunctionSet;
            for (sit = wordSet.begin(); sit != wordSet.end(); ++sit) {
                if (finalWordSet.find (*sit) != finalWordSet.end())
                    conjunctionSet.insert (*sit);
            }
            if (conjunctionSet.empty())
                return wordList;
            finalWordSet = conjunctionSet;
        }

        else {
            for (sit = wordSet.begin(); sit != wordSet.end(); ++sit) {
                finalWordSet.insert (*sit);
            }
        }

        ++conditionNum;
    }

    // Now limit the set only to those words containing the requisite number
    // of anagrams.  If some words are already in the finalWordSet, then only
    // test those words.  Otherwise, run through the map of number of anagrams
    // and pull out all words matching the conditions.
    if (!finalWordSet.empty() &&
        ((minAnagrams > 0) || (maxAnagrams < MAX_ANAGRAMS)) ||
        ((minNumVowels > 0) || (maxNumVowels < MAX_WORD_LEN)) ||
        ((minNumUniqueLetters > 0) || (maxNumUniqueLetters < MAX_WORD_LEN)) ||
        ((minPointValue > 0) || (minPointValue < 10 * MAX_WORD_LEN)))
    {
        bool testAnagrams = ((minAnagrams > 0) ||
                             (maxAnagrams < MAX_ANAGRAMS));
        bool testNumVowels = ((minNumVowels > 0) ||
                              (maxNumVowels < MAX_WORD_LEN));
        bool testNumUniqueLetters = ((minNumUniqueLetters > 0) ||
                                     (maxNumUniqueLetters < MAX_WORD_LEN));
        bool testPointValue = ((minPointValue > 0) ||
                               (minPointValue < 10 * MAX_WORD_LEN));

        set<QString> wordSet;
        for (sit = finalWordSet.begin(); sit != finalWordSet.end();
                ++sit)
        {
            QString word = *sit;

            if (testAnagrams) {
                int numAnagrams = getNumAnagrams (word);
                if ((numAnagrams < minAnagrams) || (numAnagrams > maxAnagrams))
                    continue;
            }

            if (testNumVowels) {
                int numVowels = getNumVowels (word);
                if ((numVowels < minNumVowels) || (numVowels > maxNumVowels))
                    continue;
            }

            if (testNumUniqueLetters) {
                int numUniqueLetters = getNumUniqueLetters (word);
                if ((numUniqueLetters < minNumUniqueLetters) ||
                    (numUniqueLetters > maxNumUniqueLetters))
                    continue;
            }

            if (testPointValue) {
                int pointValue = getPointValue (word);
                if ((pointValue < minPointValue) ||
                    (pointValue > maxPointValue))
                    continue;
            }

            wordSet.insert (word);
        }
        finalWordSet = wordSet;
    }

    // Transform word set into word list and return it
    for (sit = finalWordSet.begin(); sit != finalWordSet.end(); ++sit) {
        wordList << *sit;
    }

    return wordList;
}

//---------------------------------------------------------------------------
//  addDefinition
//
//! Add a word with its definition.  Parse the definition and separate its
//! parts of speech.
//
//! @param word the word
//! @param definition the definition
//---------------------------------------------------------------------------
void
WordEngine::addDefinition (const QString& word, const QString& definition)
{
    if (word.isEmpty() || definition.isEmpty())
        return;

    QRegExp posRegex (QString ("\\[(\\w+)"));
    multimap<QString, QString> defMap;
    QStringList defs = definition.split (" / ");
    QString def;
    foreach (def, defs) {
        QString pos;
        if (posRegex.indexIn (def, 0) >= 0) {
            pos = posRegex.cap (1);
        }
        defMap.insert (make_pair (pos, def));
    }
    definitions.insert (make_pair (word, defMap));
}

//---------------------------------------------------------------------------
//  replaceDefinitionLinks
//
//! Replace links in a definition with the definitions of the words they are
//! linked to.  A string is assumed to have a maximum of one link.  Links may
//! be followed recursively to the maximum depth specified.
//
//! @param definition the definition with links to be replaced
//! @param maxDepth the maximum number of recursive links to replace
//! @param useFollow true if the "follow" replacement should be used
//
//! @return a string with links replaced
//---------------------------------------------------------------------------
QString
WordEngine::replaceDefinitionLinks (const QString& definition, int maxDepth,
                                    bool useFollow) const
{
    QRegExp followRegex (QString ("\\{(\\w+)=(\\w+)\\}"));
    QRegExp replaceRegex (QString ("\\<(\\w+)=(\\w+)\\>"));

    // Try to match the follow regex and the replace regex.  If a follow regex
    // is ever matched, then the "follow" replacements should always be used,
    // even if the "replace" regex is matched in a later iteration.
    QRegExp* matchedRegex = 0;
    int index = followRegex.indexIn (definition, 0);
    if (index >= 0) {
        matchedRegex = &followRegex;
        useFollow = true;
    }
    else {
        index = replaceRegex.indexIn (definition, 0);
        matchedRegex = &replaceRegex;
    }

    if (index < 0)
        return definition;

    QString modified (definition);
    QString word = matchedRegex->cap (1);
    QString pos = matchedRegex->cap (2);

    QString replacement;
    if (!maxDepth) {
        replacement = word;
    }
    else {
        QString upper = word.toUpper();
        QString subdef = getSubDefinition (upper, pos);
        if (subdef.isEmpty()) {
            replacement = useFollow ? word : upper;
        }
        else if (useFollow) {
            replacement = (matchedRegex == &followRegex) ?
                word + " (" + subdef + ")" : subdef;
        }
        else {
            replacement = upper + ", " + getSubDefinition (upper, pos);
        }
    }

    modified.replace (index, matchedRegex->matchedLength(), replacement);
    return maxDepth ? replaceDefinitionLinks (modified, maxDepth - 1,
                                              useFollow) : modified;
}

//---------------------------------------------------------------------------
//  getSubDefinition
//
//! Return the definition associated with a word and a part of speech.  If
//! more than one definition is given for a part of speech, pick the first
//! one.
//
//! @param word the word
//! @param pos the part of speech
//
//! @return the definition substring
//---------------------------------------------------------------------------
QString
WordEngine::getSubDefinition (const QString& word, const QString& pos) const
{
    if (db.isOpen()) {
        QString definition = getDefinition (word, false);
        if (definition.isEmpty())
            return QString::null;

        QRegExp posRegex (QString ("\\[(\\w+)"));
        QStringList defs = definition.split (" / ");
        QString def;
        foreach (def, defs) {
            if ((posRegex.indexIn (def, 0) > 0) &&
                (posRegex.cap (1) == pos))
            {
                QString str = def.left (def.indexOf ("[")).simplified();
                if (!str.isEmpty())
                    return str;
            }
        }
    }

    else {
        std::map<QString,
            std::multimap<QString, QString> >::const_iterator it =
            definitions.find (word);
        if (it == definitions.end())
            return QString::null;

        const multimap<QString, QString>& mmap = it->second;
        multimap<QString, QString>::const_iterator mit = mmap.find (pos);
        if (mit == mmap.end())
            return QString::null;

        return mit->second.left (mit->second.indexOf (" ["));
    }

    return QString::null;
}
