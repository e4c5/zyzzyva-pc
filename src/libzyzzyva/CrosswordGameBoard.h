//---------------------------------------------------------------------------
// CrosswordGameBoard.h
//
// A class to represent a crossword game board.
//
// Copyright 2006 Michael W Thelen <mthelen@gmail.com>.
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

#ifndef ZYZZYVA_CROSSWORD_GAME_BOARD_H
#define ZYZZYVA_CROSSWORD_GAME_BOARD_H

#include <QChar>
#include <QList>
#include <QObject>

class CrosswordGameBoard : public QObject
{
    Q_OBJECT

    public:
    class Tile {
        public:
        Tile() : blank (false), valid (false) { }
        Tile (const QChar& c, bool b)
            : letter (c), blank (b), valid (true) { }

        void setLetter (const QChar& c) { letter = c; valid = true; }
        void setBlank (bool b) { blank = b; valid = true; }
        QChar getLetter() const { return letter; }
        bool isBlank() const { return blank; }
        bool isValid() const { return valid; }

        private:
        QChar letter;
        bool blank;
        bool valid;
    };

    enum SquareType {
        Invalid = 0,
        NoBonus,
        DoubleLetter,
        TripleLetter,
        DoubleWord,
        TripleWord
    };

    public:
    CrosswordGameBoard();
    SquareType getSquareType (int row, int col) const;
    Tile getTile (int row, int col) const;
    int getNumRows() const;
    int getNumColumns() const;

    signals:
    void changed();

    private:
    QList< QList<SquareType> > squareTypes;
    QList< QList<Tile> > tiles;
};

#endif // ZYZZYVA_CROSSWORD_GAME_BOARD_H