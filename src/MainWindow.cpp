//---------------------------------------------------------------------------
// MainWindow.cpp
//
// The main window for the word study application.
//
// Copyright 2004, 2005 Michael W Thelen <mike@pietdepsi.com>.
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

#include "MainWindow.h"
#include "AboutDialog.h"
#include "DefinitionDialog.h"
#include "DefineForm.h"
#include "HelpDialog.h"
#include "JudgeForm.h"
#include "MainSettings.h"
#include "NewQuizDialog.h"
#include "QuizEngine.h"
#include "QuizForm.h"
#include "SearchForm.h"
#include "SettingsDialog.h"
#include "WordEngine.h"
#include "WordEntryDialog.h"
#include "WordVariationDialog.h"
#include "WordVariationType.h"
#include "Auxil.h"
#include "Defs.h"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSignalMapper>
#include <QStatusBar>

MainWindow* MainWindow::instance = 0;

const QString IMPORT_FAILURE_TITLE = "Load Failed";
const QString IMPORT_COMPLETE_TITLE = "Load Complete";
const QString DEFINE_TAB_TITLE = "Define";
const QString JUDGE_TAB_TITLE = "Judge";
const QString QUIZ_TAB_TITLE = "Quiz";
const QString SEARCH_TAB_TITLE = "Search";

const QString SETTINGS_MAIN = "/Zyzzyva";
const QString SETTINGS_GEOMETRY = "/geometry";
const QString SETTINGS_GEOMETRY_X = "/x";
const QString SETTINGS_GEOMETRY_Y = "/y";
const QString SETTINGS_GEOMETRY_WIDTH = "/width";
const QString SETTINGS_GEOMETRY_HEIGHT = "/height";

using namespace Defs;

//---------------------------------------------------------------------------
//  MainWindow
//
//! Constructor.
//
//! @param parent the parent widget
//! @param f widget flags
//---------------------------------------------------------------------------
MainWindow::MainWindow (QWidget* parent, Qt::WFlags f)
    : QMainWindow (parent, f), wordEngine (new WordEngine()),
      settingsDialog (new SettingsDialog (this)),
      aboutDialog (new AboutDialog (this)),
      helpDialog (new HelpDialog (QString::null, this))
{
    // File Menu
    QMenu* fileMenu = menuBar()->addMenu ("&File");
    Q_CHECK_PTR (fileMenu);

    // New Quiz
    QAction* newQuizAction = new QAction ("New Qui&z...", this);
    Q_CHECK_PTR (newQuizAction);
    connect (newQuizAction, SIGNAL (triggered()),
             SLOT (newQuizFormInteractive()));
    fileMenu->addAction (newQuizAction);

    // New Search
    QAction* newSearchAction = new QAction ("New &Search", this);
    Q_CHECK_PTR (newSearchAction);
    connect (newSearchAction, SIGNAL (triggered()), SLOT (newSearchForm()));
    fileMenu->addAction (newSearchAction);

    // New Definition
    QAction* newDefinitionAction = new QAction ("New &Definition", this);
    Q_CHECK_PTR (newDefinitionAction);
    connect (newDefinitionAction, SIGNAL (triggered()),
             SLOT (newDefineForm()));
    fileMenu->addAction (newDefinitionAction);

    // New Word Judge
    QAction* newJudgeAction = new QAction ("New Word &Judge", this);
    Q_CHECK_PTR (newJudgeAction);
    connect (newJudgeAction, SIGNAL (triggered()), SLOT (newJudgeForm()));
    fileMenu->addAction (newJudgeAction);

    fileMenu->addSeparator();

    // Open Word List
    QAction* openWordListAction = new QAction ("&Open...", this);
    Q_CHECK_PTR (openWordListAction);
    openWordListAction->setShortcut (tr ("Ctrl+O"));
    connect (openWordListAction, SIGNAL (triggered()),
             SLOT (importInteractive()));
    fileMenu->addAction (openWordListAction);

    fileMenu->addSeparator();

    // Close Table
    QAction* closeTabAction = new QAction ("&Close Tab", this);
    Q_CHECK_PTR (closeTabAction);
    closeTabAction->setShortcut (tr ("Ctrl+W"));
    connect (closeTabAction, SIGNAL (triggered()), SLOT (closeCurrentTab()));
    fileMenu->addAction (closeTabAction);

    // Quit
    QAction* quitAction = new QAction ("&Quit", this);
    Q_CHECK_PTR (quitAction);
    connect (quitAction, SIGNAL (triggered()), qApp, SLOT (quit()));
    fileMenu->addAction (quitAction);

    // Edit Menu
    QMenu* editMenu = menuBar()->addMenu ("&Edit");
    Q_CHECK_PTR (editMenu);

    // Preferences
    QAction* editPrefsAction = new QAction ("&Preferences", this);
    Q_CHECK_PTR (editPrefsAction);
    connect (editPrefsAction, SIGNAL (triggered()), SLOT (editSettings()));
    editMenu->addAction (editPrefsAction);

    // View Menu
    QMenu* viewMenu = menuBar()->addMenu ("&View");
    Q_CHECK_PTR (viewMenu);

    // View Definition
    QAction* viewDefinitionAction = new QAction ("&Definition...", this);
    Q_CHECK_PTR (viewDefinitionAction);
    connect (viewDefinitionAction, SIGNAL (triggered()),
             SLOT (viewDefinition()));
    viewMenu->addAction (viewDefinitionAction);

    QSignalMapper* viewMapper = new QSignalMapper (this);
    Q_CHECK_PTR (viewMapper);

    // View Anagrams
    QAction* viewAnagramsAction = new QAction ("&Anagrams...", this);
    Q_CHECK_PTR (viewAnagramsAction);
    connect (viewAnagramsAction, SIGNAL (triggered()),
             viewMapper, SLOT (map()));
    viewMapper->setMapping (viewAnagramsAction, VariationAnagrams);
    viewMenu->addAction (viewAnagramsAction);

    // View Subanagrams
    QAction* viewSubanagramsAction = new QAction ("&Subanagrams...", this);
    Q_CHECK_PTR (viewSubanagramsAction);
    connect (viewSubanagramsAction, SIGNAL (triggered()),
             viewMapper, SLOT (map()));
    viewMapper->setMapping (viewSubanagramsAction, VariationSubanagrams);
    viewMenu->addAction (viewSubanagramsAction);

    // View Hooks
    QAction* viewHooksAction = new QAction ("&Hooks...", this);
    Q_CHECK_PTR (viewHooksAction);
    connect (viewHooksAction, SIGNAL (triggered()), viewMapper, SLOT (map()));
    viewMapper->setMapping (viewHooksAction, VariationHooks);
    viewMenu->addAction (viewHooksAction);

    // View Extensions
    QAction* viewExtensionsAction = new QAction ("&Extensions...", this);
    Q_CHECK_PTR (viewExtensionsAction);
    connect (viewExtensionsAction, SIGNAL (triggered()),
             viewMapper, SLOT (map()));
    viewMapper->setMapping (viewExtensionsAction, VariationExtensions);
    viewMenu->addAction (viewExtensionsAction);

    // View Anagram Hooks
    QAction* viewAnagramHooksAction = new QAction ("Anagram Hoo&ks...", this);
    Q_CHECK_PTR (viewAnagramHooksAction);
    connect (viewAnagramHooksAction, SIGNAL (triggered()),
             viewMapper, SLOT (map()));
    viewMapper->setMapping (viewAnagramHooksAction, VariationAnagramHooks);
    viewMenu->addAction (viewAnagramHooksAction);

    // View Blank Anagrams
    QAction* viewBlankAnagramsAction = new QAction ("&Blank Anagrams...",
                                                    this);
    Q_CHECK_PTR (viewBlankAnagramsAction);
    connect (viewBlankAnagramsAction, SIGNAL (triggered()),
             viewMapper, SLOT (map()));
    viewMapper->setMapping (viewBlankAnagramsAction, VariationBlankAnagrams);
    viewMenu->addAction (viewBlankAnagramsAction);

    // View Blank Matches
    QAction* viewBlankMatchesAction = new QAction ("Blank &Matches...", this);
    Q_CHECK_PTR (viewBlankMatchesAction);
    connect (viewBlankMatchesAction, SIGNAL (triggered()),
             viewMapper, SLOT (map()));
    viewMapper->setMapping (viewBlankMatchesAction, VariationBlankMatches);
    viewMenu->addAction (viewBlankMatchesAction);

    // View Transpositions
    QAction* viewTranspositionsAction = new QAction ("&Transpositions...",
                                                     this);
    Q_CHECK_PTR (viewTranspositionsAction);
    connect (viewTranspositionsAction, SIGNAL (triggered()),
             viewMapper, SLOT (map()));
    viewMapper->setMapping (viewTranspositionsAction,
                            VariationTranspositions);
    viewMenu->addAction (viewTranspositionsAction);

    // Connect View signal mappings to viewVariation
    connect (viewMapper, SIGNAL (mapped (int)), SLOT (viewVariation (int)));

    // Help Menu
    QMenu* helpMenu = menuBar()->addMenu ("&Help");
    Q_CHECK_PTR (helpMenu);

    // Help
    QAction* helpAction = new QAction ("&Help", this);
    Q_CHECK_PTR (helpAction);
    connect (helpAction, SIGNAL (triggered()), SLOT (displayHelp()));
    helpMenu->addAction (helpAction);

    // About
    QAction* aboutAction = new QAction ("&About", this);
    Q_CHECK_PTR (aboutAction);
    connect (aboutAction, SIGNAL (triggered()), SLOT (displayAbout()));
    helpMenu->addAction (aboutAction);

    tabStack = new QTabWidget (this);
    Q_CHECK_PTR (tabStack);

    closeButton = new QToolButton (tabStack);
    Q_CHECK_PTR (closeButton);
    closeButton->setUsesTextLabel (true);
    closeButton->setTextLabel ("X", false);
    tabStack->setCornerWidget (closeButton);
    closeButton->hide();
    connect (closeButton, SIGNAL (clicked()), SLOT (closeCurrentTab()));

    setCentralWidget (tabStack);

    messageLabel = new QLabel;
    Q_CHECK_PTR (messageLabel);
    statusBar()->addWidget (messageLabel, 2);

    statusLabel = new QLabel;
    Q_CHECK_PTR (statusLabel);
    statusBar()->addWidget (statusLabel, 1);
    setNumWords (0);

    readSettings (true);

    QString importFile = MainSettings::getAutoImportFile();
    if (!importFile.isEmpty())
        import (importFile);

    importStems();

    if (!instance)
        instance = this;

    setWindowTitle ("Zyzzyva");
}

//---------------------------------------------------------------------------
//  ~MainWindow
//
//! Destructor.  Save application settings.
//---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    writeSettings();
}

//---------------------------------------------------------------------------
//  importInteractive
//
//! Allow the user to import a word list from a file.
//---------------------------------------------------------------------------
void
MainWindow::importInteractive()
{
    QString file = QFileDialog::getOpenFileName (this, IMPORT_CHOOSER_TITLE,
        QDir::current().path(), "All Files (*.*)");

    if (file.isNull()) return;
    int imported = import (file);
    if (imported < 0) return;
    QMessageBox::information (this, IMPORT_COMPLETE_TITLE,
                              "Loaded " + QString::number (imported)
                              + " words.",
                              QMessageBox::Ok);
}

//---------------------------------------------------------------------------
//  newQuizFormInteractive
//
//! Create a new quiz form interactively.
//---------------------------------------------------------------------------
void
MainWindow::newQuizFormInteractive()
{
    NewQuizDialog* dialog = new NewQuizDialog (this);
    Q_CHECK_PTR (dialog);
    int code = dialog->exec();
    if (code == QDialog::Accepted) {
        newQuizForm (dialog->getQuizSpec());
    }
    delete dialog;
}

//---------------------------------------------------------------------------
//  newQuizFormInteractive
//
//! Create a new quiz form interactively, with the new quiz dialog initialized
//! from a quiz specification.
//---------------------------------------------------------------------------
void
MainWindow::newQuizFormInteractive (const QuizSpec& quizSpec)
{
    NewQuizDialog* dialog = new NewQuizDialog (this);
    Q_CHECK_PTR (dialog);
    dialog->setQuizSpec (quizSpec);
    int code = dialog->exec();
    if (code == QDialog::Accepted) {
        newQuizForm (dialog->getQuizSpec());
    }
    delete dialog;
}

//---------------------------------------------------------------------------
//  newQuizForm
//
//! Create a new quiz form directly from a quiz specification without
//! presenting the user with a quiz spec dialog.
//
//! @param quizSpec the quiz specification
//---------------------------------------------------------------------------
void
MainWindow::newQuizForm (const QuizSpec& quizSpec)
{
    QuizForm* form = new QuizForm (wordEngine);
    Q_CHECK_PTR (form);
    form->setTileTheme (MainSettings::getTileTheme());
    form->newQuiz (quizSpec);
    newTab (form, QUIZ_TAB_TITLE);
}

//---------------------------------------------------------------------------
//  newSearchForm
//
//! Create a new search form.
//---------------------------------------------------------------------------
void
MainWindow::newSearchForm()
{
    SearchForm* form = new SearchForm (wordEngine);
    Q_CHECK_PTR (form);
    newTab (form, SEARCH_TAB_TITLE);
}

//---------------------------------------------------------------------------
//  newDefineForm
//
//! Create a new word definition form.
//---------------------------------------------------------------------------
void
MainWindow::newDefineForm()
{
    DefineForm* form = new DefineForm (wordEngine);
    Q_CHECK_PTR (form);
    newTab (form, DEFINE_TAB_TITLE);
}

//---------------------------------------------------------------------------
//  newJudgeForm
//
//! Create a new word judgment form.
//---------------------------------------------------------------------------
void
MainWindow::newJudgeForm()
{
    JudgeForm* form = new JudgeForm (wordEngine);
    Q_CHECK_PTR (form);
    newTab (form, JUDGE_TAB_TITLE);
}

//---------------------------------------------------------------------------
//  editSettings
//
//! Allow the user to edit application settings.  If the user makes changes
//! and accepts the dialog, write the settings.  If the user rejects the
//! dialog, restore the settings after the dialog is closed.
//---------------------------------------------------------------------------
void
MainWindow::editSettings()
{
    if (settingsDialog->exec() == QDialog::Accepted)
        settingsDialog->writeSettings();
    else
        settingsDialog->readSettings();
    readSettings (false);
}

//---------------------------------------------------------------------------
//  viewDefinition
//
//! Allow the user to view the definition of a word.  Display a dialog asking
//! the user for the word.
//---------------------------------------------------------------------------
void
MainWindow::viewDefinition()
{
    WordEntryDialog* entryDialog = new WordEntryDialog (this);
    Q_CHECK_PTR (entryDialog);
    entryDialog->setCaption ("View Word Definition");
    entryDialog->resize (entryDialog->minimumSizeHint().width() * 2,
                         entryDialog->minimumSizeHint().height());
    int code = entryDialog->exec();
    QString word = entryDialog->getWord();
    delete entryDialog;
    if ((code != QDialog::Accepted) || word.isEmpty())
        return;

    DefinitionDialog* dialog = new DefinitionDialog (wordEngine, word, this,
                                                     Qt::WDestructiveClose);
    Q_CHECK_PTR (dialog);
    dialog->show();
}

//---------------------------------------------------------------------------
//  viewVariation
//
//! Allow the user to view variations of a word.  Display a dialog asking the
//! user for the word.
//---------------------------------------------------------------------------
void
MainWindow::viewVariation (int variation)
{
    QString caption;
    switch (variation) {
        case VariationAnagrams: caption = "View Anagrams"; break;
        case VariationSubanagrams: caption = "View Subanagrams"; break;
        case VariationHooks: caption = "View Hooks"; break;
        case VariationExtensions: caption = "View Extensions"; break;
        case VariationAnagramHooks: caption = "View Anagram Hooks"; break;
        case VariationBlankAnagrams: caption = "View Blank Anagrams"; break;
        case VariationBlankMatches: caption = "View Blank Matches"; break;
        case VariationTranspositions: caption = "View Transpositions"; break;
        default: break;
    }

    WordEntryDialog* entryDialog = new WordEntryDialog (this);
    Q_CHECK_PTR (entryDialog);
    entryDialog->setCaption (caption);
    entryDialog->resize (entryDialog->minimumSizeHint().width() * 2,
                         entryDialog->minimumSizeHint().height());
    int code = entryDialog->exec();
    QString word = entryDialog->getWord();
    delete entryDialog;
    if ((code != QDialog::Accepted) || word.isEmpty())
        return;

    WordVariationType type = static_cast<WordVariationType>(variation);
    WordVariationDialog* dialog = new WordVariationDialog (wordEngine, word,
                                                           type, this,
                                                           Qt::WDestructiveClose);
    Q_CHECK_PTR (dialog);
    dialog->show();
}

//---------------------------------------------------------------------------
//  displayAbout
//
//! Display an About screen.
//---------------------------------------------------------------------------
void
MainWindow::displayAbout()
{
    aboutDialog->exec();
}

//---------------------------------------------------------------------------
//  displayHelp
//
//! Display a Help screen.
//---------------------------------------------------------------------------
void
MainWindow::displayHelp()
{
    helpDialog->showPage(Auxil::getHelpDir() + "/index.html");
}

//---------------------------------------------------------------------------
//  closeCurrentTab
//
//! Close the currently open tab.  If no other tabs exist, hide the button
//! used for closing tabs.
//---------------------------------------------------------------------------
void
MainWindow::closeCurrentTab()
{
    QWidget* w = tabStack->currentPage();
    if (!w)
        return;

    tabStack->removePage (w);
    delete w;
    if (tabStack->count() == 0)
        closeButton->hide();
}

//---------------------------------------------------------------------------
//  setNumWords
//
//! Update the label displaying the number of words loaded.
//
//! @param num the new number of words loaded
//---------------------------------------------------------------------------
void
MainWindow::setNumWords (int num)
{
    statusLabel->setText (QString::number (num) + " words loaded");
}

//---------------------------------------------------------------------------
//  readSettings
//
//! Read application settings.
//---------------------------------------------------------------------------
void
MainWindow::readSettings (bool useGeometry)
{
    MainSettings::readSettings();

    if (useGeometry)
        setGeometry (MainSettings::getMainWindowX(),
                     MainSettings::getMainWindowY(),
                     MainSettings::getMainWindowWidth(),
                     MainSettings::getMainWindowHeight());

    // Main font
    QFont mainFont;
    QString fontStr = MainSettings::getMainFont();
    bool mainFontOk = true;
    if (mainFont.fromString (fontStr))
        qApp->setFont (mainFont);
    else {
        qWarning ("Cannot set font: " + fontStr);
        mainFontOk = false;
    }

    // Word list font
    QFont font;
    fontStr = MainSettings::getWordListFont();
    if (font.fromString (fontStr))
        qApp->setFont (font, "WordTableView");
    else
        qWarning ("Cannot set font: " + fontStr);

    // Set word list headers back to main font
    if (mainFontOk)
        qApp->setFont (mainFont, "QHeaderView");

    // Quiz label font
    // XXX: Reinstate this once it's know how to change the font of canvas
    // text items via QApplication::setFont
    //fontStr = MainSettings::getQuizLabelFont();
    //if (font.fromString (fontStr))
    //    // FIXME: How to get QCanvasText items to update their font?
    //    qApp->setFont (font, "QCanvasText");
    //else
    //    qWarning ("Cannot set font: " + fontStr);

    // Word input font
    fontStr = MainSettings::getWordInputFont();
    if (font.fromString (fontStr)) {
        qApp->setFont (font, "WordLineEdit");
        qApp->setFont (font, "WordTextEdit");
    }
    else
        qWarning ("Cannot set font: " + fontStr);

    // Definition font
    fontStr = MainSettings::getDefinitionFont();
    if (font.fromString (fontStr)) {
        qApp->setFont (font, "DefinitionBox");
        qApp->setFont (font, "DefinitionLabel");
    }
    else
        qWarning ("Cannot set font: " + fontStr);

    // Set tile theme for all quiz forms
    QString tileTheme = MainSettings::getTileTheme();
    int count = tabStack->count();
    for (int i = 0; i < count; ++i) {
        ActionForm* form = static_cast<ActionForm*> (tabStack->page (i));
        ActionForm::ActionFormType type = form->getType();
        if (type == ActionForm::QuizFormType) {
            QuizForm* quizForm = static_cast<QuizForm*> (form);
            quizForm->setTileTheme (tileTheme);
        }
    }

    // FIXME: Figure out how to incorporate this with WordTableModel
    //WordListViewItem::setSortByLength
    //    (MainSettings::getWordListSortByLength());
}

//---------------------------------------------------------------------------
//  writeSettings
//
//! Write application settings.
//---------------------------------------------------------------------------
void
MainWindow::writeSettings()
{
    MainSettings::setMainWindowX (x());
    MainSettings::setMainWindowY (y());
    MainSettings::setMainWindowWidth (width());
    MainSettings::setMainWindowHeight (height());
    MainSettings::writeSettings();
}

//---------------------------------------------------------------------------
//  newTab
//
//! Create and display a new tab.
//
//! @param widget the widget to display
//! @param title the title of the tab
//---------------------------------------------------------------------------
void
MainWindow::newTab (QWidget* widget, const QString& title)
{
    tabStack->addTab (widget, title);
    tabStack->showPage (widget);
    closeButton->show();
}

//---------------------------------------------------------------------------
//  import
//
//! Import words from a file.
//
//! @return the number of imported words
//---------------------------------------------------------------------------
int
MainWindow::import (const QString& file)
{
    QString err;
    QApplication::setOverrideCursor (Qt::waitCursor);
    int imported = wordEngine->importFile (file, true, &err);
    QApplication::restoreOverrideCursor();

    if (imported < 0)
        QMessageBox::warning (this, IMPORT_FAILURE_TITLE, err);
    else
        setNumWords (imported);
    return imported;
}

//---------------------------------------------------------------------------
//  importStems
//
//! Import stems.  XXX: Right now this is hard-coded to load certain North
//! American stems.  Should be more flexible.
//
//! @return the number of imported stems
//---------------------------------------------------------------------------
int
MainWindow::importStems()
{
    QStringList stemFiles;
    stemFiles << (Auxil::getWordsDir() + "/north-american/6-letter-stems.txt");
    stemFiles << (Auxil::getWordsDir() + "/north-american/7-letter-stems.txt");

    QString err;
    QApplication::setOverrideCursor (Qt::waitCursor);
    QStringList::iterator it;
    int totalImported = 0;
    for (it = stemFiles.begin(); it != stemFiles.end(); ++it) {
        int imported = wordEngine->importStems (*it, &err);
        totalImported += imported;
    }
    QApplication::restoreOverrideCursor();

    return totalImported;
}
