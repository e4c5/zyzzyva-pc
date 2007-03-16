//---------------------------------------------------------------------------
// SearchForm.cpp
//
// A form for searching for words, patterns, anagrams, etc.
//
// Copyright 2004, 2005, 2006, 2007 Michael W Thelen <mthelen@gmail.com>.
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

#include "SearchForm.h"
#include "MainSettings.h"
#include "SearchSpecForm.h"
#include "WordEngine.h"
#include "WordTableModel.h"
#include "WordTableView.h"
#include "ZPushButton.h"
#include "Defs.h"
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTimer>

using namespace Defs;

const QString TITLE_PREFIX = "Search";

//---------------------------------------------------------------------------
//  SearchForm
//
//! Constructor.
//
//! @param e the word engine
//! @param parent the parent widget
//! @param f widget flags
//---------------------------------------------------------------------------
SearchForm::SearchForm(WordEngine* e, QWidget* parent, Qt::WFlags f)
    : ActionForm(SearchFormType, parent, f), wordEngine(e)
{
    QHBoxLayout* mainHlay = new QHBoxLayout(this);
    Q_CHECK_PTR(mainHlay);
    mainHlay->setMargin(MARGIN);
    mainHlay->setSpacing(SPACING);

    QVBoxLayout* specVlay = new QVBoxLayout;
    Q_CHECK_PTR (specVlay);
    specVlay->setSpacing(SPACING);
    mainHlay->addLayout(specVlay);

    specForm = new SearchSpecForm;
    Q_CHECK_PTR(specForm);
    connect(specForm, SIGNAL(returnPressed()), SLOT(search()));
    connect(specForm, SIGNAL(contentsChanged()), SLOT(specChanged()));
    specVlay->addWidget(specForm);

    lowerCaseCbox = new QCheckBox("Use &lower-case letters for wildcard "
                                  "matches");
    Q_CHECK_PTR(lowerCaseCbox);
    specVlay->addWidget(lowerCaseCbox);

    QHBoxLayout* buttonHlay = new QHBoxLayout;
    Q_CHECK_PTR(buttonHlay);
    buttonHlay->setSpacing(SPACING);
    specVlay->addLayout(buttonHlay);

    searchButton = new ZPushButton("&Search");
    Q_CHECK_PTR(searchButton);
    searchButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(searchButton, SIGNAL(clicked()), SLOT(search()));
    buttonHlay->addWidget(searchButton);

    resultView = new WordTableView(wordEngine);
    Q_CHECK_PTR(resultView);
    specVlay->addWidget(resultView, 1);

    resultModel = new WordTableModel(wordEngine, this);
    Q_CHECK_PTR(resultModel);
    connect(resultModel, SIGNAL(wordsChanged()),
            resultView, SLOT(resizeItemsToContents()));
    resultView->setModel(resultModel);

    specChanged();
    QTimer::singleShot(0, specForm, SLOT(selectInputArea()));
}

//---------------------------------------------------------------------------
//  getIcon
//
//! Returns the current icon.
//
//! @return the current icon
//---------------------------------------------------------------------------
QIcon
SearchForm::getIcon() const
{
    return QIcon(":/search-icon");
}

//---------------------------------------------------------------------------
//  getTitle
//
//! Returns the current title string.
//
//! @return the current title string
//---------------------------------------------------------------------------
QString
SearchForm::getTitle() const
{
    return TITLE_PREFIX;
}

//---------------------------------------------------------------------------
//  getStatusString
//
//! Returns the current status string.
//
//! @return the current status string
//---------------------------------------------------------------------------
QString
SearchForm::getStatusString() const
{
    return statusString;
}

//---------------------------------------------------------------------------
//  isSaveEnabled
//
//! Determine whether the save action should be enabled for this form.
//
//! @return true if save should be enabled, false otherwise
//---------------------------------------------------------------------------
bool
SearchForm::isSaveEnabled() const
{
    return (resultModel->rowCount() > 0);
}

//---------------------------------------------------------------------------
//  saveRequested
//
//! Called when a save action is requested.
//---------------------------------------------------------------------------
void
SearchForm::saveRequested()
{
    resultView->exportRequested();
}

//---------------------------------------------------------------------------
//  search
//
//! Search for the word or pattern in the edit area, and display the results
//! in the list box.
//---------------------------------------------------------------------------
void
SearchForm::search()
{
    SearchSpec spec = specForm->getSearchSpec();
    if (spec.conditions.empty())
        return;

    searchButton->setEnabled(false);
    resultModel->removeRows(0, resultModel->rowCount());

    statusString = "Searching...";
    emit statusChanged(statusString);
    qApp->processEvents();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QStringList wordList = wordEngine->search(specForm->getSearchSpec(), false);

    if (!wordList.empty()) {

        // Check for Anagram or Subanagram conditions, and only group by
        // alphagrams if one of them is present
        bool hasAnagramCondition = false;
        bool hasProbabilityCondition = false;
        QListIterator<SearchCondition> it (spec.conditions);
        while (it.hasNext()) {
            SearchCondition::SearchType type = it.next().type;
            if ((type == SearchCondition::AnagramMatch) ||
                (type == SearchCondition::SubanagramMatch) ||
                (type == SearchCondition::NumAnagrams))
            {
                hasAnagramCondition = true;
            }

            if ((type == SearchCondition::ProbabilityOrder) ||
                (type == SearchCondition::LimitByProbabilityOrder))
            {
                hasProbabilityCondition = true;
            }
        }

        // Create a list of WordItem objects from the words
        QList<WordTableModel::WordItem> wordItems;
        QString word;
        foreach (word, wordList) {

            QString wildcard;
            if (hasAnagramCondition) {
                // Get wildcard characters
                QList<QChar> wildcardChars;
                for (int i = 0; i < word.length(); ++i) {
                    QChar c = word[i];
                    if (c.isLower())
                        wildcardChars.append(c);
                }
                if (!wildcardChars.isEmpty()) {
                    qSort(wildcardChars);
                    QChar c;
                    foreach (c, wildcardChars)
                        wildcard.append(c.toUpper());
                }
            }

            QString wordUpper = word.toUpper();

            // Convert to all caps if necessary
            if (!lowerCaseCbox->isChecked())
                word = wordUpper;

            WordTableModel::WordItem wordItem
                (word, WordTableModel::WordNormal, wildcard);

            int probOrder = wordEngine->getProbabilityOrder(wordUpper);
            wordItem.setProbabilityOrder(probOrder);

            wordItems.append(wordItem);
        }

        // FIXME: Probably not the right way to get alphabetical sorting instead
        // of alphagram sorting
        bool origGroupByAnagrams = MainSettings::getWordListGroupByAnagrams();
        if (!hasAnagramCondition)
            MainSettings::setWordListGroupByAnagrams(false);
        if (hasProbabilityCondition)
            MainSettings::setWordListSortByProbabilityOrder(true);
        resultModel->addWords(wordItems);
        MainSettings::setWordListSortByProbabilityOrder(false);
        if (!hasAnagramCondition)
            MainSettings::setWordListGroupByAnagrams(origGroupByAnagrams);
    }

    updateResultTotal(wordList.size());
    emit saveEnabledChanged(!wordList.empty());

    QWidget* focusWidget = QApplication::focusWidget();
    QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    if (lineEdit) {
        lineEdit->setSelection(0, lineEdit->text().length());
    }
    else {
        specForm->selectInputArea();
    }

    searchButton->setEnabled(true);
    QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
//  specChanged
//
//! Called when the contents of the search spec form change.  Enable or
//! disable the Search button appropriately.
//---------------------------------------------------------------------------
void
SearchForm::specChanged()
{
    searchButton->setEnabled(specForm->isValid());
}

//---------------------------------------------------------------------------
//  updateResultTotal
//
//! Display the number of words currently in the search results.
//! @param num the number of words
//---------------------------------------------------------------------------
void
SearchForm::updateResultTotal(int num)
{
    QString wordStr = QString::number(num) + " word";
    if (num != 1)
        wordStr += "s";
    statusString = "Search found " + wordStr;
    emit statusChanged(statusString);
}
